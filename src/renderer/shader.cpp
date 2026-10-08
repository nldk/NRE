#include "shader.h"

#include <fstream>
#include <vector>

#include "context.h"
#include "errorHandeling.h"

namespace NRE::Shader {
    Result<void> Shader::create(std::string path) {
        auto c = context();
        if (c) {
            if (ErrorHandeling::handleError(Error{ErrorCode::AlreadyInitialized,"Renderer already initialized"})) {
                return std::unexpected(Error{ErrorCode::AlreadyInitialized,"Renderer already initialized"});
            };
        }
        std::ifstream ifs(path,std::ios::binary | std::ios::ate);
        if (!ifs.is_open()) {
            if (ErrorHandeling::handleError(Error{ErrorCode::FileNotFound,"File " + path + " not found"})) {
                return std::unexpected(Error{ErrorCode::FileNotFound,"File " + path + " not found"});
            };
        }
        size_t size = ifs.tellg();
        std::vector<uint32_t> code(size / sizeof(uint32_t));
        ifs.seekg(0);
        ifs.read(reinterpret_cast<char*>(code.data()), size);
        VkShaderModuleCreateInfo info = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        info.codeSize = size;
        info.pCode = code.data();
        vkCreateShaderModule(c->vulkan.device, &info, nullptr, &shader);
        return {};
    }
}
