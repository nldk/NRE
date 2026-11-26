#pragma once
#include <utility>
#include <vulkan/vulkan_core.h>
#include <vector>

#include "gpu/descriptorSet.h"
#include "renderer.h"
namespace NRE {
    struct VertexLayout {
        std::vector<VkVertexInputBindingDescription>   bindings;
        std::vector<VkVertexInputAttributeDescription> attributes;
    };
    struct PushConstants {
        glm::mat4 model;      // offset 0
        glm::mat4 inversModelMatrix;   // offset 64
    };
    static_assert(sizeof(PushConstants) <= 128, "exceeds guaranteed push constant size");
    struct Pipeline {
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline       pipeline = VK_NULL_HANDLE;
        // Empty: call createPipeline() to make it, to get its Result.
        Pipeline() = default;
        // The defaults are for scene geometry. The skybox uses no culling and depthCompare LESS_OR_EQUAL without
        // depthWrite, so it only fills the pixels still at the cleared far plane.
        Pipeline(Handel<Shader> vertexShader, Handel<Shader> fragmentShader, const VertexLayout& layout = {},
                 VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT, VkCompareOp depthCompare = VK_COMPARE_OP_LESS,
                 bool depthWrite = true) {
            createPipeline(vertexShader, fragmentShader, layout, cullMode, depthCompare, depthWrite);
        };
        Result<void> createPipeline(Handel<Shader> vertexShader, Handel<Shader> fragmentShader, const VertexLayout& layout,
                                    VkCullModeFlags cullMode, VkCompareOp depthCompare, bool depthWrite);
        Pipeline(const Pipeline&) = delete;
        Pipeline& operator=(const Pipeline&) = delete;
        Pipeline(Pipeline&& o) noexcept
            : pipelineLayout(std::exchange(o.pipelineLayout, VK_NULL_HANDLE)),
              pipeline(std::exchange(o.pipeline, VK_NULL_HANDLE)) {}
        Pipeline& operator=(Pipeline&& o) noexcept {
            if (this != &o) {
                std::swap(pipelineLayout, o.pipelineLayout);
                std::swap(pipeline, o.pipeline);
            }
            return *this;
        }
        ~Pipeline();
    };

}
