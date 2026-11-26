#include "pipeline/shader.h"

#include <fstream>
#include <string>
#include <filesystem>
#include <vector>

#include "core/context.h"
#include "core/errorHandling.h"

namespace NRE {
    Result<void> Shader::create(std::string path) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        std::ifstream ifs(path,std::ios::binary | std::ios::ate);
        if (!ifs.is_open()) {
            auto error = Error{ErrorCode::FileNotFound, "createShader(): shader file not found: '"
                               + std::filesystem::absolute(path).string() + "'"};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            return {};
        }
        size_t size = ifs.tellg();
        // SPIR-V is a list of 32-bit words starting with the magic number 0x07230203.
        uint32_t magic = 0;
        ifs.seekg(0);
        ifs.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        if (size == 0 || size % 4 != 0 || magic != 0x07230203) {
            auto error = Error{ErrorCode::UnsupportedFormat, "createShader(): '" + path + "' isn't compiled SPIR-V. "
                               "Pass the .spv file glslc produces (e.g. vertex.vert.spv), not the GLSL source."};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            return {};
        }
        std::vector<uint32_t> code(size / sizeof(uint32_t));
        ifs.seekg(0);
        ifs.read(reinterpret_cast<char*>(code.data()), size);
        return create(code, "'" + path + "'");
    }
    Result<void> Shader::create(std::span<const uint32_t> code, const std::string& name) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        VkShaderModuleCreateInfo info = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        info.codeSize = code.size_bytes();
        info.pCode = code.data();
        if (VkResult r = vkCreateShaderModule(c->vulkan.device, &info, nullptr, &shader); r != VK_SUCCESS) {
            shader = VK_NULL_HANDLE;
            auto error = Error{ErrorCode::ShaderCreationFailed, "createShader(): failed to create a shader module from "
                               + name + ": " + std::string(ErrorHandeling::vkResultName(r))};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
        }
        return {};
    }
    BuiltinShader::BuiltinShader(std::span<const uint32_t> code, ShaderType type)
        : handle(context()->manager.create(Shader(code, type))) {}

    BuiltinShader::~BuiltinShader() {
        if (auto c = context()) c->manager.destroy(handle, true);
    }

    Result<void> Shader::destroy() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        if (shader) {
            vkDestroyShaderModule(c->vulkan.device, shader, nullptr);
        }
        shader = VK_NULL_HANDLE;
        return {};
    }
}
