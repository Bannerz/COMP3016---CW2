#pragma once

#include <cstdio>
#include <vector>
#include <GL/glew.h>
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <string>
#include <unordered_map>

#include "MapTextures.h"

class MapPreview {
    // Values match the map material modes in basic.frag.
    enum TextureMode { ColourOnly, AuthoredUV, Projected, Canvas, Thatch };

    struct Surface {
        GLuint texture = 0;
        TextureMode mode = ColourOnly;
    };
    struct Mesh {
        GLuint vao = 0, vbo = 0, ebo = 0;
        GLsizei indexCount = 0;
        aiColor4D tint{ 1, 1, 1, 1 };
        Surface surface;
    };

    // Many placements can point at the same uploaded mesh.
    struct Placement {
        unsigned meshIndex;
        aiMatrix4x4 transform;
        aiMatrix4x4 closedTransform;
    };

    std::vector<Mesh> meshes;
    std::vector<GLuint> textures;
    std::vector<Placement> placements;
    std::unordered_map<std::string, std::vector<std::size_t>> doorPlacements;

    Surface fallbackSurface(const aiScene& scene, const aiMaterial& material, MapTextures& loader)
    {
        aiString materialName;
        material.Get(AI_MATKEY_NAME, materialName);
        const std::string name = materialName.C_Str();
        const auto has = [&](const char* part) { return name.find(part) != std::string::npos; };

        const char* tile = nullptr;
        if (has("Masonry |") || has("masonry |") || has("soot-blackened stone") ||
            has("soot-darkened stone") || has("weathering collar") || has("mossy joints")) {
            tile = "Castle | blue-grey rubble | packed albedo";
        }
        else if (has("Timber |") || has("stacked charred logs")) {
            tile = "Packed | dark oak";
        }
        else if (has("Limewash |") || has("limewashed infill")) {
            tile = "Lime";
        }
        else if (has(" slate")) {
            tile = "Slate";
        }
        else if (has("Canvas") || has("Sewn canopy") || has(" wool")) {
            return {0, Canvas};
        }
        else if (has("Thatch") || has("weathered straw") || has("bound straw")) {
            return {0, Thatch};
        }
        if (!tile) return {};
        const auto texture = loader.projected(scene, tile, textures);
        return {texture, texture ? Projected : ColourOnly};
    }

    void collectNodes(const aiNode* node, const aiMatrix4x4& parent)
    {
        // Assimp stores each node relative to its parent. Accumulate the world transform.
        aiMatrix4x4 world = parent * node->mTransformation;

        const std::string nodeName = node->mName.C_Str();
        for (unsigned i = 0; i < node->mNumMeshes; ++i) {
            const unsigned meshIndex = node->mMeshes[i];
            if (nodeName.compare(0, 7, "Door | ") == 0 && meshes[meshIndex].indexCount > 0) {
                doorPlacements[nodeName].push_back(placements.size());
            }
            placements.push_back({ meshIndex, world, world });
        }

        for (unsigned i = 0; i < node->mNumChildren; ++i)
            collectNodes(node->mChildren[i], world);
    }

public:
    bool load(const char* path)
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate);
        if (!scene || !scene->mRootNode) {
            std::fprintf(stderr, "Map import failed: %s\n",
                importer.GetErrorString());
            return false;
        }

        MapTextures textureLoader;
        std::size_t authoredMeshes = 0;
        std::size_t fallbackMeshes = 0;
        meshes.resize(scene->mNumMeshes);
        for (unsigned i = 0; i < scene->mNumMeshes; ++i) {
            const aiMesh* source = scene->mMeshes[i];
            Mesh& mesh = meshes[i];

            const auto& material = *scene->mMaterials[source->mMaterialIndex];
            material.Get(AI_MATKEY_BASE_COLOR, mesh.tint);
            unsigned uvChannel = 0;
            const auto texture = textureLoader.authored(*scene, *source, uvChannel, textures);
            if (texture) {
                mesh.surface = {texture, AuthoredUV};
                ++authoredMeshes;
            }
            else {
                mesh.surface = fallbackSurface(*scene, material, textureLoader);
                if (mesh.surface.mode != ColourOnly) ++fallbackMeshes;
            }

            // Keep the same position, UV and RGB attributes as basic.vert.
            std::vector<float> vertices;
            vertices.reserve(source->mNumVertices * 8);
            for (unsigned j = 0; j < source->mNumVertices; ++j) {
                const auto& p = source->mVertices[j];
                aiColor4D c = source->HasVertexColors(0)
                    ? source->mColors[0][j] : aiColor4D(1, 1, 1, 1);
                const aiVector3D uv = texture
                    ? source->mTextureCoords[uvChannel][j] : aiVector3D{};
                // Assimp flips glTF V. Undo it because WIC returns the top row first.
                vertices.insert(vertices.end(),
                    { p.x, p.y, p.z, uv.x, texture ? 1.0f - uv.y : 0.0f, c.r, c.g, c.b });
            }

            std::vector<unsigned int> indices;
            indices.reserve(source->mNumFaces * 3);
            for (unsigned j = 0; j < source->mNumFaces; ++j) {
                const aiFace& face = source->mFaces[j];
                if (face.mNumIndices == 3) {
                    indices.insert(indices.end(),
                        face.mIndices, face.mIndices + 3);
                }
            }

            if (indices.empty()) continue;

            glGenVertexArrays(1, &mesh.vao);
            glBindVertexArray(mesh.vao);
            glGenBuffers(1, &mesh.vbo);
            glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
                vertices.data(), GL_STATIC_DRAW);

            constexpr GLsizei stride = 8 * sizeof(float);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                reinterpret_cast<const void*>(3 * sizeof(float)));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride,
                reinterpret_cast<const void*>(5 * sizeof(float)));
            glEnableVertexAttribArray(2);

            glGenBuffers(1, &mesh.ebo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                indices.size() * sizeof(unsigned int),
                indices.data(), GL_STATIC_DRAW);
            mesh.indexCount = static_cast<GLsizei>(indices.size());
        }

        // Copy the placement data before the local importer releases its scene.
        collectNodes(scene->mRootNode, aiMatrix4x4{});
        std::printf("Map loaded: %zu meshes, %zu placements\n",
            meshes.size(), placements.size());
        std::printf("Map textures: %zu GPU textures, %zu UV-textured meshes, %zu fallback meshes\n",
            textures.size(), authoredMeshes, fallbackMeshes);
        return !placements.empty();
    }

    bool hasDoor(const std::string& nodeName) const
    {
        return doorPlacements.find(nodeName) != doorPlacements.end();
    }

    void setDoorAngle(const std::string& nodeName, const aiVector3D& hinge, float angle)
    {
        aiMatrix4x4 toHinge, rotation, fromHinge;

        aiMatrix4x4::Translation(hinge, toHinge);
        aiMatrix4x4::RotationY(angle, rotation);
        aiMatrix4x4::Translation(
            aiVector3D(-hinge.x, -hinge.y, -hinge.z), fromHinge
        );

        const aiMatrix4x4 swing = toHinge * rotation * fromHinge;

        const auto door = doorPlacements.find(nodeName);
        if (door == doorPlacements.end()) return;
        for (std::size_t index : door->second) {
            auto& placement = placements[index];
            placement.transform = swing * placement.closedTransform;
        }
    }

    void draw(GLint modelLocation, GLint tintLocation, GLint textureModeLocation) const
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUniform1i(textureModeLocation, ColourOnly);
        Surface boundSurface;

        // Draw each placement with its own transform and the referenced mesh buffers.
        for (const Placement& placement : placements) {
            const Mesh& mesh = meshes[placement.meshIndex];
            if (mesh.indexCount == 0) continue;

            if (boundSurface.texture != mesh.surface.texture) {
                glBindTexture(GL_TEXTURE_2D, mesh.surface.texture);
            }
            if (boundSurface.mode != mesh.surface.mode) {
                glUniform1i(textureModeLocation, mesh.surface.mode);
            }
            boundSurface = mesh.surface;

            const aiMatrix4x4& m = placement.transform;
            const float rows[] = {
                m.a1, m.a2, m.a3, m.a4,
                m.b1, m.b2, m.b3, m.b4,
                m.c1, m.c2, m.c3, m.c4,
                m.d1, m.d2, m.d3, m.d4
            };

            // GL_TRUE tells OpenGL that these values are laid out by row.
            glUniformMatrix4fv(modelLocation, 1, GL_TRUE, rows);
            glUniform3f(tintLocation, mesh.tint.r, mesh.tint.g, mesh.tint.b);
            glBindVertexArray(mesh.vao);
            glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr);
        }
    }

    void release()
    {
        for (const Mesh& mesh : meshes) {
            glDeleteVertexArrays(1, &mesh.vao);
            glDeleteBuffers(1, &mesh.vbo);
            glDeleteBuffers(1, &mesh.ebo);
        }
        glDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());
        textures.clear();
    }
};
