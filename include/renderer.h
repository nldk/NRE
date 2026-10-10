#ifndef VULKANRENDERER_RENDERER_H
#define VULKANRENDERER_RENDERER_H
#include <expected>
#include <ostream>
#include <renderer_export.h>
#include <string>
#include <string_view>

#include <cstdint>

namespace NRE {
    enum class ShaderType {
        FRAGMENT_SHADER,
        VERTEX_SHADER,
        GEOMETRY_SHADER,
        COMPUTE_SHADER,
    };
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
        ErrorCreatingVulkanInstance,
        ErrorCreatingSwapchain,
        ErrorRecreatingSwapchain,
        ErrorCreatingPipeline,
        WindowCreationFailed,
        FileNotFound,
        WrongMode,
        NotInitialized,
        AlreadyInitialized,
        HandelOutOfDateOrNonExistend,
        UnsupportedFormat,
        ModelLoadFailed,
        FailedToCreateAllocator,
        UnknownError,
    };
    struct WindowConfig {
        int width,height;
        std::string title;
        bool resizable;
    };
    struct RenderConfig {
        RenderMode mode;
        RenderErrorPolicy errorPolicy;
        bool manualFramePresentationAndSubmission;
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
            case ErrorCode::ErrorCreatingVulkanInstance: return "ErrorCreatingVulkanInstance";
            case ErrorCode::ErrorCreatingSwapchain: return "ErrorCreatingSwapchain";
            case ErrorCode::ErrorRecreatingSwapchain: return "ErrorRecreatingSwapchain";
            case ErrorCode::HandelOutOfDateOrNonExistend: return "HandelOutOfDateOrNonExistend";
            case ErrorCode::ErrorCreatingPipeline: return "ErrorCreatingPipeline";
            case ErrorCode::UnsupportedFormat: return "UnsupportedFormat";
            case ErrorCode::ModelLoadFailed: return "ModelLoadFailed";
        }
        return "UnknownError. Not in enum.";
    }

    inline std::ostream& operator<<(std::ostream& os, ErrorCode code) {
        return os << toString(code);
    }

    inline std::ostream& operator<<(std::ostream& os, const Error& error) {
        return os << error.code << ": " << error.error;
    }
    template <typename T>
    using Result = std::expected<T, Error>;

    template <class T>
    class Handel {
        public:
        uint32_t index;
        uint32_t generation;
    };

    struct Shader;
    class Pipeline;
    struct Model;

    RENDERER_API Result<void> setRenderConfig(RenderConfig config);
    RENDERER_API Result<void> init();
    RENDERER_API Result<void> shutdown();
    RENDERER_API Result<void> renderFrame();
    RENDERER_API Result<void> presentFrame();
    RENDERER_API Result<void> submitFrame();
    RENDERER_API Result<bool> windowShouldClose();
    RENDERER_API Result<void> pollEvents();
    RENDERER_API Result<Handel<Shader>> createShader(std::string shaderPath,ShaderType type);
    RENDERER_API Result<void> destroyShader(Handel<Shader> shader);
    RENDERER_API Result<Handel<Pipeline>> createPipeline(Handel<Shader> vertexShader,Handel<Shader> fragmentShader);
    RENDERER_API Result<void> destroyPipeline(Handel<Pipeline> pipeline);
    RENDERER_API Result<void> bindPipeline(Handel<Pipeline> pipeline);
    RENDERER_API Result<Handel<Model>> createModel(std::string modelPath);
    RENDERER_API Result<void> destroyModel(Handel<Model> model);
};

#endif //VULKANRENDERER_RENDERER_H
