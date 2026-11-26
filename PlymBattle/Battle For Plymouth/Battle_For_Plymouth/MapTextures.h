#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <GL/glew.h>
#include <assimp/scene.h>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <map>
#include <stdexcept>
#include <tuple>
#include <vector>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "windowscodecs.lib")

// Use Windows Imaging Component to decode the GLB's compressed base-colour images.
class MapTextures {
    using Factory = Microsoft::WRL::ComPtr<IWICImagingFactory>;
    Factory factory;
    HRESULT apartment = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    std::map<std::tuple<unsigned, GLenum, GLenum>, GLuint> cache;

    static void check(HRESULT result, const char* operation)
    {
        if (FAILED(result)) throw std::runtime_error(operation);
    }

    static GLenum wrapMode(aiTextureMapMode mode)
    {
        if (mode == aiTextureMapMode_Mirror) return GL_MIRRORED_REPEAT;
        if (mode == aiTextureMapMode_Clamp || mode == aiTextureMapMode_Decal) {
            return GL_CLAMP_TO_EDGE;
        }
        return GL_REPEAT;
    }

    GLuint upload(const aiTexture& image, GLenum wrapS, GLenum wrapT)
    {
        if (image.mHeight != 0 || image.mWidth == 0) {
            throw std::runtime_error("expected a compressed embedded image");
        }
        if (!factory) {
            check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(factory.GetAddressOf())), "create Windows image decoder");
        }

        using Microsoft::WRL::ComPtr;
        ComPtr<IWICStream> stream;
        ComPtr<IWICBitmapDecoder> decoder;
        ComPtr<IWICBitmapFrameDecode> frame;
        ComPtr<IWICFormatConverter> rgba;
        check(factory->CreateStream(stream.GetAddressOf()), "create image stream");
        check(stream->InitializeFromMemory(reinterpret_cast<BYTE*>(image.pcData),
            image.mWidth), "read embedded image");
        check(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad,
            decoder.GetAddressOf()), "decode embedded image");
        check(decoder->GetFrame(0, frame.GetAddressOf()), "read image frame");
        check(factory->CreateFormatConverter(rgba.GetAddressOf()), "create RGBA converter");
        check(rgba->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom),
            "convert image to RGBA");
        UINT width = 0, height = 0;
        check(rgba->GetSize(&width, &height), "read image dimensions");
        GLint gpuLimit = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &gpuLimit);
        const auto byteCount = std::uint64_t(width) * height * 4;
        if (!width || !height || width > unsigned(gpuLimit) || height > unsigned(gpuLimit) ||
            byteCount > (std::numeric_limits<UINT>::max)()) {
            throw std::runtime_error("image exceeds supported dimensions");
        }
        std::vector<BYTE> pixels(static_cast<std::size_t>(byteCount));
        check(rgba->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()),
            pixels.data()), "read image pixels");

        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, width, height,
            0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glGenerateMipmap(GL_TEXTURE_2D);
        if (GLEW_EXT_texture_filter_anisotropic) {
            GLfloat limit = 1.0f;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &limit);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                limit < 8.0f ? limit : 8.0f);
        }
        if (glGetError() != GL_NO_ERROR) {
            glDeleteTextures(1, &texture);
            throw std::runtime_error("upload image to OpenGL");
        }
        return texture;
    }

    GLuint load(const aiScene& scene, unsigned index, GLenum wrapS, GLenum wrapT,
        std::vector<GLuint>& owned)
    {
        const auto key = std::make_tuple(index, wrapS, wrapT);
        auto entry = cache.emplace(key, 0);
        if (!entry.second) return entry.first->second;

        const auto& image = *scene.mTextures[index];
        try {
            const auto texture = upload(image, wrapS, wrapT);
            owned.push_back(texture);
            entry.first->second = texture;
            return texture;
        }
        catch (const std::runtime_error& error) {
            std::fprintf(stderr, "Map texture %s: %s\n", image.mFilename.C_Str(), error.what());
            return 0;
        }
    }

public:
    MapTextures() = default;
    MapTextures(const MapTextures&) = delete;
    MapTextures& operator=(const MapTextures&) = delete;

    ~MapTextures()
    {
        factory.Reset();
        if (SUCCEEDED(apartment)) CoUninitialize();
    }

    GLuint authored(const aiScene& scene, const aiMesh& mesh, unsigned& uvChannel,
        std::vector<GLuint>& owned)
    {
        const auto& material = *scene.mMaterials[mesh.mMaterialIndex];
        aiString path;
        aiTextureMapMode wrap[2] = {aiTextureMapMode_Wrap, aiTextureMapMode_Wrap};
        if (material.GetTexture(aiTextureType_BASE_COLOR, 0, &path, nullptr,
                &uvChannel, nullptr, nullptr, wrap) != AI_SUCCESS ||
            !mesh.HasTextureCoords(uvChannel)) {
            return 0;
        }
        const auto image = scene.GetEmbeddedTextureAndIndex(path.C_Str());
        if (!image.first) {
            std::fprintf(stderr, "Map texture missing from GLB: %s\n", path.C_Str());
            return 0;
        }
        return load(scene, static_cast<unsigned>(image.second),
            wrapMode(wrap[0]), wrapMode(wrap[1]), owned);
    }

    GLuint projected(const aiScene& scene, const char* imageName, std::vector<GLuint>& owned)
    {
        for (unsigned i = 0; i < scene.mNumTextures; ++i) {
            if (scene.mTextures[i]->mFilename == aiString(imageName)) {
                return load(scene, i, GL_REPEAT, GL_REPEAT, owned);
            }
        }
        return 0;
    }
};
