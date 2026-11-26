#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include <glm/glm.hpp>

#include "gpu/buffer.h"
#include "renderer.h"
#include "gpu/materials.h"
#include "pipeline/pipeline.h"

#include "scene/scene.h"

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
        uint32_t material = UINT32_MAX;
        Buffer vertexBuffer;
        Buffer indexBuffer;
    };

    struct Model {
        std::vector<Mesh> meshes;
        std::vector<Handel<Material>> materials;

        Result<void> uploadMeshes();

        void releaseGpuMeshes();

        Model() = default;

        // Pushes the matrices, then per mesh binds its material (set 1) and sets the cull mode (none for
        // double-sided materials, back faces otherwise) before drawing it. Set 0 must already be bound.
        // cullMode: the cull mode the command buffer has now. The cull mode is only set where it differs, and
        // cullMode is updated. Start a pass (or a newly bound pipeline) with unknownCullMode.
        static constexpr VkCullModeFlags unknownCullMode = VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
        Result<void> draw(VkCommandBuffer commandBuffer,Pipeline &pipeline, const glm::mat4& modelMatrix, VkCullModeFlags& cullMode);
        // The shadow pass (lighting/shadows.cpp): pushes the model matrix (offset 0), then per mesh binds depthOnly,
        // or alphaMasked and the mesh's material (set 1) for alpha-masked materials, and draws it. The two pipelines
        // must share a layout; set 0, the rest of the push constants and the cull mode are the caller's.
        // boundPipeline: the pipeline bound now. It's only rebound where it differs, and boundPipeline is updated.
        void drawDepthOnly(VkCommandBuffer commandBuffer, const Pipeline& depthOnly, const Pipeline& alphaMasked,
                           const glm::mat4& modelMatrix, VkPipeline& boundPipeline);
        [[nodiscard]] std::vector<Handel<Material>> getMaterials() const{ return materials; };
        Model(const Model&) = delete;
        Model& operator=(const Model&) = delete;
        Model(Model&&) noexcept = default;
        Model& operator=(Model&&) noexcept = default;
    };

    struct Instance {
        Handel<Model> model;
        Transform transform;
        bool visible = true;
        bool castsShadows = true;   // setCastsShadows: whether it's drawn into the sun's shadow maps
        std::vector<Handel<Scene>> scenes;
    };
    Result<Model> loadModel(const std::string& path);
}
