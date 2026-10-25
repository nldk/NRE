#pragma once
#include <cstdint>
#include <expected>
#include <source_location>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>

#include "renderer.h"

namespace NRE::ErrorHandeling {
    bool handleError(Error error);
    [[noreturn]] void panic();

    // "translateInstance()" or "PerFrameBuffer::write()", from the compiler's full signature.
    // Every message starts with this, so you can see which call failed.
    std::string functionName(const std::source_location& location);
    // "VK_ERROR_OUT_OF_DEVICE_MEMORY"
    std::string_view vkResultName(VkResult result);

    // Reports a NotInitialized error for the function at location. Kept out of line so callers
    // don't each carry the string-building code.
    [[gnu::cold, gnu::noinline]] std::unexpected<Error> notInitialized(const std::source_location& location);

    // A HandelOutOfDateOrNonExistend error that says which function got the handle, whether it was empty
    // or stale, and which function makes a valid one. kind is e.g. "instance", creator e.g. "createInstance()".
    [[gnu::cold, gnu::noinline]] Error badHandle(std::string_view kind, uint32_t index, uint32_t generation,
                                                 std::string_view creator, const std::source_location& location);
    template <class T>
    Error badHandle(std::string_view kind, Handel<T> handle, std::string_view creator, const std::source_location& location) {
        return badHandle(kind, handle.index, handle.generation, creator, location);
    }
}

// Returns a NotInitialized error from the calling function if the context is null, i.e. setRenderConfig()
// hasn't been called. The message names the calling function.
// Release builds (NDEBUG) leave the check out to save code size, so calling the API before init is undefined there.
// NRE_REQUIRE_CONTEXT_AT is for helpers like getInstance(): they take the caller's std::source_location
// so the message names the API function the user called, not the helper.
#ifdef NDEBUG
    #define NRE_REQUIRE_CONTEXT(c) ((void)(c))
    #define NRE_REQUIRE_CONTEXT_AT(c, location) ((void)(c), (void)(location))
#else
    #define NRE_REQUIRE_CONTEXT_AT(c, location) \
        do { if (!(c)) [[unlikely]] return ::NRE::ErrorHandeling::notInitialized(location); } while (0)
    #define NRE_REQUIRE_CONTEXT(c) NRE_REQUIRE_CONTEXT_AT(c, std::source_location::current())
#endif
