// NRE::renderer_dynamic: loadRenderer() / unloadRenderer(), and every NRE:: function defined as a call
// through the function table of the loaded library. Apps link this instead of NRE::renderer.

#include "renderer_dynamic.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace NRE {
    namespace {
        const FunctionTable* table = nullptr;
        void* library = nullptr;

#ifdef NRE_UNSAFE_API
        constexpr uint32_t unsafeApi = 1;
#else
        constexpr uint32_t unsafeApi = 0;
#endif

        void* openLibrary(const std::string& path, std::string& error) {
#ifdef _WIN32
            HMODULE handle = LoadLibraryA(path.c_str());
            if (!handle) error = "LoadLibrary failed with error " + std::to_string(GetLastError());
            return reinterpret_cast<void*>(handle);
#else
            void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
            if (!handle) {
                const char* message = dlerror();
                error = message ? message : "dlopen failed";
            }
            return handle;
#endif
        }

        void* findSymbol(void* lib, const char* name) {
#ifdef _WIN32
            return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(lib), name));
#else
            return dlsym(lib, name);
#endif
        }

        void closeLibrary(void* lib) {
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(lib));
#else
            dlclose(lib);
#endif
        }

        // Empty if it can't be found.
        std::filesystem::path executableDirectory() {
#ifdef _WIN32
            wchar_t buffer[MAX_PATH];
            DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
            if (length == 0 || length == MAX_PATH) return {};
            return std::filesystem::path(buffer).parent_path();
#elif defined(__linux__)
            std::error_code error;
            std::filesystem::path exe = std::filesystem::read_symlink("/proc/self/exe", error);
            return error ? std::filesystem::path{} : exe.parent_path();
#else
            return {};
#endif
        }

        // Where an empty path looks, in order.
        std::vector<std::string> candidates(const std::string& path) {
            if (!path.empty()) return {path};
            std::vector<std::string> list;
            if (std::filesystem::path dir = executableDirectory(); !dir.empty()) list.push_back((dir / NRE_LIBRARY_FILE).string());
            list.push_back(NRE_LIBRARY_BUILD_PATH);
            list.push_back(NRE_LIBRARY_FILE);   // the system's search path (LD_LIBRARY_PATH, RUNPATH, PATH, ...)
            return list;
        }

        Error loadError(const std::string& message) {
            return Error{ErrorCode::LibraryLoadFailed, "loadRenderer(): " + message};
        }

        // Why t can't be used by this app, or empty if it can.
        std::string incompatibility(const FunctionTable& t) {
            if (t.versionMajor != NRE_VERSION_MAJOR || t.versionMinor != NRE_VERSION_MINOR) {
                return std::format("it's NRE {}.{}.{}, but this app was built against {}.{}.{}. Only patch versions "
                                   "can differ.", t.versionMajor, t.versionMinor, t.versionPatch,
                                   NRE_VERSION_MAJOR, NRE_VERSION_MINOR, NRE_VERSION_PATCH);
            }
            if (t.unsafeApi != unsafeApi) {
                return t.unsafeApi ? "it was built with NRE_UNSAFE_API and this app without it."
                                   : "this app was built with NRE_UNSAFE_API and the library without it.";
            }
            if (t.size != sizeof(FunctionTable)) {
                return std::format("its function table is {} bytes and this app's is {}: it was built from different "
                                   "headers than this app.", t.size, sizeof(FunctionTable));
            }
            return {};
        }

#ifndef NDEBUG
        [[noreturn]] void notLoaded(const char* name) {
            std::fprintf(stderr, "[NRE] %s() was called before NRE::loadRenderer(). With NRE::renderer_dynamic, "
                                 "load the renderer library first.\n", name);
            std::abort();
        }
#endif
    }

    Result<void> loadRenderer(const std::string& path) {
        if (table) {
            return std::unexpected(loadError("a renderer is already loaded. Call shutdown() and unloadRenderer() first."));
        }
        std::string tried;
        for (const std::string& candidate : candidates(path)) {
            std::string error;
            void* lib = openLibrary(candidate, error);
            if (!lib) {
                tried += "\n  " + candidate + ": " + error;
                continue;
            }
            // Found one: from here on, a problem is an error instead of a reason to try the next candidate.
            auto getTable = reinterpret_cast<NreGetFunctionTableFn>(findSymbol(lib, "nreGetFunctionTable"));
            if (!getTable) {
                closeLibrary(lib);
                return std::unexpected(loadError(candidate + " has no nreGetFunctionTable(), so it isn't an NRE renderer "
                                                 "that supports loading at runtime."));
            }
            const FunctionTable* t = getTable();
            if (std::string reason = incompatibility(*t); !reason.empty()) {
                closeLibrary(lib);
                return std::unexpected(loadError("can't use " + candidate + ": " + reason));
            }
            table = t;
            library = lib;
            return {};
        }
        return std::unexpected(loadError("couldn't load the renderer library. Tried:" + tried));
    }

    void unloadRenderer() {
        if (!library) return;
        closeLibrary(library);
        library = nullptr;
        table = nullptr;
    }

    bool isRendererLoaded() {
        return table != nullptr;
    }

#ifdef NDEBUG
#define NRE_CHECK_LOADED(name)
#else
#define NRE_CHECK_LOADED(name) if (!table) notLoaded(#name);
#endif

    // The forwarders: NRE::name(...) calls table->name(...).
#define NRE_FUNCTION(ret, name, params, args) \
    ret name params { NRE_CHECK_LOADED(name) return table->name args; }
#define NRE_UNSAFE_FUNCTION(ret, name, params, args)
#include "renderer_functions.inl"
#undef NRE_FUNCTION
#undef NRE_UNSAFE_FUNCTION

#ifdef NRE_UNSAFE_API
    namespace Unsafe {
#define NRE_FUNCTION(ret, name, params, args)
#define NRE_UNSAFE_FUNCTION(ret, name, params, args) \
    ret name params { NRE_CHECK_LOADED(Unsafe::name) return table->unsafe_##name args; }
#include "renderer_functions.inl"
#undef NRE_FUNCTION
#undef NRE_UNSAFE_FUNCTION
    }
#endif
#undef NRE_CHECK_LOADED
}
