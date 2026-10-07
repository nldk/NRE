#include "errorHandeling.h"

#include <iostream>
#include <ostream>

#include "context.h"
#include "renderer.h"
#include "VkBootstrap.h"

namespace Renderer::ErrorHandeling {
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
    void panic() {
        std::abort();
    }
}
