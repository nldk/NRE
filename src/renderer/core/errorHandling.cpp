#include "core/errorHandling.h"

#include <iostream>
#include <ostream>

#include "core/context.h"
#include "renderer.h"
#include "VkBootstrap.h"

namespace NRE::ErrorHandeling {
    bool handleError(Error error) {
        std::cerr << "[ErrorHandeling]: " << error << std::endl;
        if (!context()) {
            std::cerr << "[ErrorHandeling]: no renderer context (setRenderConfig not called), defaulting to Panic policy\n";
            panic();
        }
        if (context()->config.errorPolicy == RenderErrorPolicy::Panic) {
            panic();
        }
        return context()->config.errorPolicy == RenderErrorPolicy::Return;
    };
    std::unexpected<Error> notInitialized(std::string_view message) {
        Error error{ErrorCode::NotInitialized, std::string(message)};
        handleError(error);
        return std::unexpected(std::move(error));
    }
    void panic() {
        std::abort();
    }
}
