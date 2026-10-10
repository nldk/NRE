#pragma once
#include <utility>
#include <vulkan/vulkan_core.h>

#include "renderer.h"
namespace NRE {
    struct Pipeline {
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline       pipeline = VK_NULL_HANDLE;
        Pipeline(Handel<Shader> vertexShader, Handel<Shader> fragmentShader) {
            createPipeline(vertexShader,fragmentShader);
        };
        Result<void> createPipeline(Handel<Shader> vertexShader, Handel<Shader> fragmentShader);
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
