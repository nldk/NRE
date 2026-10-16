#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include <glm/glm.hpp>

#include "buffer.h"
#include "renderer.h"
#include "pipeline.h"

#include "scene.h"

namespace NRE {
    struct Vertex {
        glm::vec3 position{0.0f};
        glm::vec3 normal{0.0f};
        glm::vec2 uv{0.0f};
        static VertexLayout layout() {
            return {
                {{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX}},
                {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)},
                 {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)},
                 {2, 0, VK_FORMAT_R32G32_SFLOAT,    offsetof(Vertex, uv)}},
            };
        }
    };
    static_assert(sizeof(Vertex) == 32, "Vertex layout changed - update the pipeline");
    static_assert(std::is_standard_layout_v<Vertex>);
    struct Mesh {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        Buffer vertexBuffer;
        Buffer indexBuffer;
    };
    struct Model {
        std::vector<Mesh> meshes;
        Transform transform;
        std::vector<Handel<Scene>> scenes ;
        bool visible = true;

        Result<void> uploadMeshes();

        void releaseGpuMeshes();

        Model() = default;
        void draw(VkCommandBuffer commandBuffer,Pipeline &pipeline, glm::mat4 viewProj);

        Model(const Model&) = delete;
        Model& operator=(const Model&) = delete;
        Model(Model&&) noexcept = default;
        Model& operator=(Model&&) noexcept = default;
    };

    Result<Model> loadModel(const std::string& path);
}
