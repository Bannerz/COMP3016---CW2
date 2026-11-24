#pragma once

#include <cstdio>
#include <vector>
#include <GL/glew.h>
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

class MapPreview {
    struct Mesh {
        GLuint vao = 0, vbo = 0, ebo = 0;
        GLsizei indexCount = 0;
        aiColor4D tint{ 1, 1, 1, 1 };
    };

    struct Placement {
        unsigned meshIndex;
        aiMatrix4x4 transform;
    };

    std::vector<Mesh> meshes;
    std::vector<Placement> placements;

    void collectNodes(const aiNode* node, const aiMatrix4x4& parent)
    {
        aiMatrix4x4 world = parent * node->mTransformation;

        for (unsigned i = 0; i < node->mNumMeshes; ++i)
            placements.push_back({ node->mMeshes[i], world });

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

        meshes.resize(scene->mNumMeshes);
        for (unsigned i = 0; i < scene->mNumMeshes; ++i) {
            const aiMesh* source = scene->mMeshes[i];
            Mesh& mesh = meshes[i];

            std::vector<float> vertices;
            vertices.reserve(source->mNumVertices * 6);
            for (unsigned j = 0; j < source->mNumVertices; ++j) {
                const auto& p = source->mVertices[j];
                aiColor4D c = source->HasVertexColors(0)
                    ? source->mColors[0][j] : aiColor4D(1, 1, 1, 1);
                vertices.insert(vertices.end(), { p.x, p.y, p.z, c.r, c.g, c.b });
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

            scene->mMaterials[source->mMaterialIndex]->Get(
                AI_MATKEY_BASE_COLOR, mesh.tint);

            glGenVertexArrays(1, &mesh.vao);
            glBindVertexArray(mesh.vao);
            glGenBuffers(1, &mesh.vbo);
            glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
                vertices.data(), GL_STATIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                6 * sizeof(float), nullptr);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                reinterpret_cast<const void*>(3 * sizeof(float)));
            glEnableVertexAttribArray(2);

            glGenBuffers(1, &mesh.ebo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                indices.size() * sizeof(unsigned int),
                indices.data(), GL_STATIC_DRAW);
            mesh.indexCount = static_cast<GLsizei>(indices.size());
        }

        collectNodes(scene->mRootNode, aiMatrix4x4{});
        std::printf("Map loaded: %zu meshes, %zu placements\n",
            meshes.size(), placements.size());
        return !placements.empty();
    }

    void draw(GLint modelLocation, GLint tintLocation) const
    {
        for (const Placement& placement : placements) {
            const Mesh& mesh = meshes[placement.meshIndex];
            if (mesh.indexCount == 0) continue;

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
    }
};
