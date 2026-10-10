#pragma once
#include <string>
#include <utility>
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
        VkShaderModule shader = VK_NULL_HANDLE;
        Result<void> create(std::string path);
        Result<void> destroy();
        Shader(const Shader&) = delete;
        Shader& operator=(const Shader&) = delete;
        Shader(Shader&& o) noexcept
            : type(o.type), shader(std::exchange(o.shader, VK_NULL_HANDLE)) {}
        Shader& operator=(Shader&& o) noexcept {
            if (this != &o) {
                destroy();
                type = o.type;
                shader = std::exchange(o.shader, VK_NULL_HANDLE);
            }
            return *this;
        }
        ~Shader() {
            destroy();
        };
    };
}

