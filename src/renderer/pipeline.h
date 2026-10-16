#pragma once
#include <utility>
#include <vulkan/vulkan_core.h>
#include <vector>
#include "renderer.h"
namespace NRE {
    struct VertexLayout {
        std::vector<VkVertexInputBindingDescription>   bindings;
        std::vector<VkVertexInputAttributeDescription> attributes;
    };
    struct PushConstants {
        glm::mat4 model;      // offset 0
        glm::mat4 viewProj;   // offset 64
    };
    static_assert(sizeof(PushConstants) <= 128, "exceeds guaranteed push constant size");
    struct Pipeline {
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline       pipeline = VK_NULL_HANDLE;
        Pipeline(Handel<Shader> vertexShader, Handel<Shader> fragmentShader,const VertexLayout& layout = {}) {
            createPipeline(vertexShader,fragmentShader,layout);
        };
        Result<void> createPipeline(Handel<Shader> vertexShader, Handel<Shader> fragmentShader,const VertexLayout& layout);
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
