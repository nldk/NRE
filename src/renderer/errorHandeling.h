#pragma once
#include <string_view>

#include "renderer.h"

namespace NRE::ErrorHandeling {
    bool handleError(Error error);
    [[noreturn]] inline void panic();
}
