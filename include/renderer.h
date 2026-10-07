#ifndef VULKANRENDERER_RENDERER_H
#define VULKANRENDERER_RENDERER_H
#include <expected>
#include <ostream>
#include <renderer_export.h>
#include <string>
#include <string_view>

namespace Renderer {
    enum class RenderMode {
        Render_3D,
        Render_2D,
    };
    enum class RenderErrorPolicy {
        Ignore,
        Return,
        Panic,
    };
    enum class ErrorCode {
        UnsupportedGPU,
        ErrorCreatingVulcanInstance,
        ErrorCreatingSwapchain,
        ErrorRecreatingSwapchain,
        WindowCreationFailed,
        FileNotFound,
        WrongMode,
        NotInitialized,
        AlreadyInitialized,
        UnknownError,
    };
    struct WindowConfig {
        int width,height;
        std::string title;
    };
    struct RenderConfig {
        RenderMode mode;
        RenderErrorPolicy errorPolicy;
        WindowConfig windowConfig;
    };

    struct Error {
        ErrorCode code;
        std::string error;
    };

    constexpr std::string_view toString(ErrorCode code) {
        switch (code) {
            case ErrorCode::UnsupportedGPU:       return "UnsupportedGPU";
            case ErrorCode::WindowCreationFailed: return "WindowCreationFailed";
            case ErrorCode::FileNotFound:         return "FileNotFound";
            case ErrorCode::WrongMode:            return "WrongMode";
            case ErrorCode::NotInitialized:       return "NotInitialized";
            case ErrorCode::AlreadyInitialized:   return "AlreadyInitialized";
            case ErrorCode::UnknownError:         return "UnknownError";
            case ErrorCode::ErrorCreatingVulcanInstance: return "ErrorCreatingVulcanInstance";
            case ErrorCode::ErrorCreatingSwapchain: return "ErrorCreatingSwapchain";
            case ErrorCode::ErrorRecreatingSwapchain: return "ErrorRecreatingSwapchain";
        }
        return "UnknownError";
    }

    inline std::ostream& operator<<(std::ostream& os, ErrorCode code) {
        return os << toString(code);
    }

    inline std::ostream& operator<<(std::ostream& os, const Error& error) {
        return os << error.code << ": " << error.error;
    }
    template <typename T>
    using Result = std::expected<T, Error>;
    RENDERER_API Result<void> setRenderConfig(RenderConfig config);
    RENDERER_API Result<void> init();
    RENDERER_API Result<void> shutdown();
    RENDERER_API Result<void> renderFrame();
};

#endif //VULKANRENDERER_RENDERER_H
