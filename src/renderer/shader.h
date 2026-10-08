#pragma once
#include <string>
#include <vulkan/vulkan_core.h>

#include "errorHandeling.h"
#include "renderer.h"

namespace NRE::Shader {
    struct Shader {
        Shader(std::string path) {
            create(path);
        }
        VkShaderModule shader;
        Result<void> create(std::string path);
        Result<void> destroy();
    };
}

