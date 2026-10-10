#pragma once
#include <vulkan/vulkan_core.h>

#include "renderer.h"
namespace NRE {
    class Pipeline {
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline       pipeline = VK_NULL_HANDLE;
    public:
        Result<void> createPipeline(Handel<Shader> vertexShader, Handel<Shader> fragmentShader);
    };

}
