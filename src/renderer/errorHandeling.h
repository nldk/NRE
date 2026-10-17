#pragma once
#include <expected>
#include <string_view>

#include "renderer.h"

namespace NRE::ErrorHandeling {
    bool handleError(Error error);
    [[noreturn]] void panic();
    // Reports a NotInitialized error. Kept out of line so callers don't each carry the string-building code.
    [[gnu::cold, gnu::noinline]] std::unexpected<Error> notInitialized(std::string_view message);
}

// Returns a NotInitialized error from the calling function if the context is null.
// Release builds (NDEBUG) leave the check out to save code size, so calling the API before init is undefined there.
#ifdef NDEBUG
    #define NRE_REQUIRE_CONTEXT(c, message) ((void)(c))
#else
    #define NRE_REQUIRE_CONTEXT(c, message) \
        do { if (!(c)) [[unlikely]] return ::NRE::ErrorHandeling::notInitialized(message); } while (0)
#endif
