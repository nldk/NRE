#pragma once
#include <string_view>

#include "renderer.h"

namespace Renderer::ErrorHandeling {
    bool handleError(Error error);
    [[noreturn]] inline void panic();
}
