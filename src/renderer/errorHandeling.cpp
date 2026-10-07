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
            std::cerr << "[ErrorHangling]: error in errorhandeling the context is not defined so init wasn't run default config is panic\n";
            panic();
        }
        return context()->config.errorPolicy == RenderErrorPolicy::Return;
    };
    void panic() {
        std::abort();
    }
}
