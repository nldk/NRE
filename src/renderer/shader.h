#pragma once
#include <string>
#include <vulkan/vulkan_core.h>

#include "errorHandeling.h"
#include "renderer.h"

namespace NRE {
    struct Shader {
        ShaderType type;
        Shader(std::string path, ShaderType type) {
            create(path);
            this->type = type;
        }
        VkShaderModule shader;
        Result<void> create(std::string path);
        Result<void> destroy();
    };
}

