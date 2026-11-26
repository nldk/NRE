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
    // The scene's colour target: linear HDR, tone mapped into the swapchain format at the end of the frame
    // (see post/toneMap.h). Pipelines draw into it unless they ask for another format. Here rather than in
    // core/context.h because the Pipeline default needs it and context.h includes this header.
    constexpr VkFormat hdrFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    struct Pipeline {
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline       pipeline = VK_NULL_HANDLE;
        // Empty: call createPipeline() to make it, to get its Result.
        Pipeline() = default;
        // The defaults are for scene geometry. The skybox uses no culling and depthCompare GREATER_OR_EQUAL without
        // depthWrite, so it only fills the pixels still at the cleared far plane. Depth is reverse-Z (cleared to 0, nearer is greater).
        // colorFormat: the HDR scene target by default; the tone-map pass draws in the swapchain format.
        Pipeline(Handel<Shader> vertexShader, Handel<Shader> fragmentShader, const VertexLayout& layout = {},
                 VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT, VkCompareOp depthCompare = VK_COMPARE_OP_GREATER,
                 bool depthWrite = true, VkFormat colorFormat = hdrFormat) {
            createPipeline(vertexShader, fragmentShader, layout, cullMode, depthCompare, depthWrite, colorFormat);
        };
        Result<void> createPipeline(Handel<Shader> vertexShader, Handel<Shader> fragmentShader, const VertexLayout& layout,
                                    VkCullModeFlags cullMode, VkCompareOp depthCompare, bool depthWrite,
                                    VkFormat colorFormat = hdrFormat);
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
