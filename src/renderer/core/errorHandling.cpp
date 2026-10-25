#include "core/errorHandling.h"

#include <iostream>
#include <ostream>
#include <vulkan/vk_enum_string_helper.h>

#include "core/context.h"
#include "renderer.h"
#include "VkBootstrap.h"

namespace NRE::ErrorHandeling {
    bool handleError(Error error) {
        std::cerr << "[NRE] " << error << std::endl;
        if (!context()) {
            std::cerr << "[NRE] no renderer context (setRenderConfig() wasn't called), so the error policy is Panic\n";
            panic();
        }
        if (context()->config.errorPolicy == RenderErrorPolicy::Panic) {
            panic();
        }
        return context()->config.errorPolicy == RenderErrorPolicy::Return;
    };

    std::string functionName(const std::source_location& location) {
        // GCC gives e.g. "NRE::Result<void> NRE::translateInstance(NRE::Handel<NRE::Instance>, glm::vec3)".
        // The name is what's between the return type (ends at the last top-level space) and the parameter
        // list (the first top-level '('); '<...>' can hold both spaces and parentheses, so skip those.
        std::string_view signature = location.function_name();
        size_t start = 0, end = signature.size();
        int depth = 0;
        for (size_t i = 0; i < signature.size(); ++i) {
            char ch = signature[i];
            if (ch == '<') ++depth;
            else if (ch == '>') --depth;
            else if (depth == 0 && ch == ' ') start = i + 1;
            else if (depth == 0 && ch == '(') { end = i; break; }
        }
        std::string_view name = signature.substr(start, end - start);
        if (name.starts_with("NRE::")) name.remove_prefix(5);
        return std::string(name) + "()";
    }

    std::string_view vkResultName(VkResult result) {
        return string_VkResult(result);
    }

    std::unexpected<Error> notInitialized(const std::source_location& location) {
        Error error{ErrorCode::NotInitialized, functionName(location) + " was called before setRenderConfig(). "
                                               "Call setRenderConfig() and then init() first."};
        handleError(error);
        return std::unexpected(std::move(error));
    }

    Error badHandle(std::string_view kind, uint32_t index, uint32_t generation, std::string_view creator,
                    const std::source_location& location) {
        std::string message = functionName(location) + ": ";
        if (index == UINT32_MAX) {
            message += "the " + std::string(kind) + " handle is empty (default-constructed). "
                       "Use the handle returned by " + std::string(creator) + ".";
        } else {
            message += "the " + std::string(kind) + " handle (index " + std::to_string(index) + ", generation "
                       + std::to_string(generation) + ") is out of date: that " + std::string(kind)
                       + " was destroyed, or the handle didn't come from " + std::string(creator) + ".";
        }
        return Error{ErrorCode::HandelOutOfDateOrNonExistend, std::move(message)};
    }

    void panic() {
        std::abort();
    }
}
