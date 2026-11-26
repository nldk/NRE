#pragma once
#include <span>
#include <string>
#include <utility>
#include <vulkan/vulkan_core.h>

#include "core/errorHandling.h"
#include "renderer.h"

namespace NRE {
    struct Shader {
        ShaderType type;
        Shader(std::string path, ShaderType type) {
            create(path);
            this->type = type;
        }
        // From SPIR-V already in memory, e.g. the renderer's built-in shaders.
        Shader(std::span<const uint32_t> code, ShaderType type) {
            create(code, "built-in shader");
            this->type = type;
        }
        VkShaderModule shader = VK_NULL_HANDLE;
        Result<void> create(std::string path);
        // name: what error messages call it.
        Result<void> create(std::span<const uint32_t> code, const std::string& name);
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

    // A built-in shader from SPIR-V embedded in librenderer (see CMakeLists.txt), in the resource manager while this
    // lives. A pipeline doesn't need its modules once it's made, so keep one only around the pipeline's creation:
    //     BuiltinShader vertex(skyboxVert, ShaderType::VERTEX_SHADER);   // skyboxVert: the embedded words
    //     pipeline.createPipeline(vertex.handle, ...);
    struct BuiltinShader {
        Handel<Shader> handle;
        BuiltinShader(std::span<const uint32_t> code, ShaderType type);
        ~BuiltinShader();
        BuiltinShader(const BuiltinShader&) = delete;
        BuiltinShader& operator=(const BuiltinShader&) = delete;
    };
}

