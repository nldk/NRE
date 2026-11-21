#ifndef VULKANRENDERER_RENDERER_DYNAMIC_H
#define VULKANRENDERER_RENDERER_DYNAMIC_H

// Optional: load librenderer at runtime (dlopen / LoadLibrary) instead of linking it.
//
// Link NRE::renderer_dynamic instead of NRE::renderer, and call NRE::loadRenderer() before anything else.
// Every NRE:: function then calls into the loaded library, so the rest of the code doesn't change:
//
//     #include "renderer_dynamic.h"
//
//     if (auto loaded = NRE::loadRenderer(); !loaded) { std::cerr << loaded.error() << '\n'; return 1; }
//     NRE::setRenderConfig(...);
//     NRE::init();
//     ...
//     NRE::shutdown();
//     NRE::unloadRenderer();
//
// The library has to be the same major.minor version the app was built with (it's checked), built with the same
// NRE_UNSAFE_API setting and the same compiler and standard library, since the API passes C++ types.

#include <cstdint>
#include <string>

#include "renderer.h"
#ifdef NRE_UNSAFE_API
#include "renderer_unsafe.h"
#endif
#include "renderer_version.h"

namespace NRE {
    // Loads the renderer library and points every NRE:: function at it.
    // An empty path loads the library this app was built against (NRE_LIBRARY_FILE, e.g. librenderer.so.0.1):
    // first next to the executable, then where it was built, then the system's library search path.
    // Fails with ErrorCode::LibraryLoadFailed if it can't be loaded or isn't compatible.
    Result<void> loadRenderer(const std::string& path = {});
    // Call it after shutdown(). Afterwards loadRenderer() can load the same or another build.
    void unloadRenderer();
    bool isRendererLoaded();

    // What nreGetFunctionTable() returns. The fields before the function pointers keep their layout in every
    // version, so a loader can always read them and refuse an incompatible library.
    struct FunctionTable {
        uint32_t size;            // sizeof(FunctionTable) in the library
        uint32_t versionMajor;
        uint32_t versionMinor;
        uint32_t versionPatch;
        uint32_t unsafeApi;       // 1 if the library was built with NRE_UNSAFE_API

#define NRE_FUNCTION(ret, name, params, args) ret (*name) params;
#define NRE_UNSAFE_FUNCTION(ret, name, params, args) ret (*unsafe_##name) params;
#include "renderer_functions.inl"
#undef NRE_FUNCTION
#undef NRE_UNSAFE_FUNCTION
    };
}

// The library's one C entry point. C linkage, so its name is the same for every compiler.
extern "C" RENDERER_API const NRE::FunctionTable* nreGetFunctionTable();
using NreGetFunctionTableFn = const NRE::FunctionTable* (*)();

#endif //VULKANRENDERER_RENDERER_DYNAMIC_H
