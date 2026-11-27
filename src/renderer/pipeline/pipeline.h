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
        // The defaults are for scene geometry. The sky pass uses depthCompare GREATER_OR_EQUAL without depthWrite, so
        // it only fills the pixels still at the cleared far plane. Depth is reverse-Z (cleared to 0, nearer is greater).
        // colorFormat: the HDR scene target by default; the tone-map pass draws in the swapchain format.
        // The cull mode is dynamic state: whoever draws sets it with vkCmdSetCullMode first (Model::draw per mesh,
        // from the material's doubleSided; the built-in passes VK_CULL_MODE_NONE after binding their pipeline).
        Pipeline(Handel<Shader> vertexShader, Handel<Shader> fragmentShader, const VertexLayout& layout = {},
                 VkCompareOp depthCompare = VK_COMPARE_OP_GREATER, bool depthWrite = true, VkFormat colorFormat = hdrFormat) {
            createPipeline(vertexShader, fragmentShader, layout, depthCompare, depthWrite, colorFormat);
        };
        Result<void> createPipeline(Handel<Shader> vertexShader, Handel<Shader> fragmentShader, const VertexLayout& layout,
                                    VkCompareOp depthCompare, bool depthWrite, VkFormat colorFormat = hdrFormat);
        // Depth only (no colour attachment), for shadow maps: standard depth (cleared to 1, LESS_OR_EQUAL), depth
        // clamping (geometry in front of the near plane writes depth 0 instead of being clipped) and a depth bias.
        // fragmentShader may be empty: the pipeline then has only the vertex stage.
        Result<void> createDepthOnlyPipeline(Handel<Shader> vertexShader, Handel<Shader> fragmentShader, const VertexLayout& layout,
                                             float depthBiasConstant, float depthBiasSlope);
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
    private:
        // Both of the above. colorFormat VK_FORMAT_UNDEFINED: no colour attachment. fragment may be VK_NULL_HANDLE.
        Result<void> create(VkShaderModule vertex, VkShaderModule fragment, const VertexLayout& layout, VkCompareOp depthCompare,
                            bool depthWrite, VkFormat colorFormat, bool depthClamp, float depthBiasConstant, float depthBiasSlope);
    };

}
