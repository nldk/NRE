#ifndef VULKANRENDERER_RENDERER_H
#define VULKANRENDERER_RENDERER_H
#include <expected>
#include <ostream>
#include <renderer_export.h>
#include <string>
#include <string_view>

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

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
        BufferCreationFailed,
        UploadFailed,
        NoPipelineBound,
        NoSceneBound,
        UnknownError,
    };
    struct WindowConfig {
        int width,height;
        std::string title;
        bool resizable;
        // Appends "- 144 FPS (6.94 ms)" to the title, updated twice a second.
        bool showFpsInTitle = true;
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
            case ErrorCode::FailedToCreateAllocator: return "FailedToCreateAllocator";
            case ErrorCode::BufferCreationFailed: return "BufferCreationFailed";
            case ErrorCode::UploadFailed: return "UploadFailed";
            case ErrorCode::NoPipelineBound: return "NoPipelineBound";
            case ErrorCode::NoSceneBound: return "NoSceneBound";
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
        // A default-constructed handle refers to nothing, so lookups with it fail.
        uint32_t index = UINT32_MAX;
        uint32_t generation = 0;
        bool operator==(const Handel&) const = default;
    };

    // Position/rotation/scale of an object in world space. Applied as scale, then rotation, then translation.
    struct Transform {
        glm::vec3 position{0.0f};
        glm::quat rotation = glm::identity<glm::quat>();
        glm::vec3 scale{1.0f};
        glm::mat4 matrix() const {
            return glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
        }
    };

    struct Shader;
    class Pipeline;
    struct Model;
    class Scene;

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
    RENDERER_API Result<void> setModelTransform(Handel<Model> model, const Transform& transform);
    RENDERER_API Result<Transform> getModelTransform(Handel<Model> model);
    RENDERER_API Result<glm::mat4> getModelMatrix(Handel<Model> model);
    RENDERER_API Result<void> translateModel(Handel<Model> model, glm::vec3 offset);
    RENDERER_API Result<void> setVisibility(Handel<Model> model, bool visible);
    RENDERER_API Result<Handel<Scene>> createScene();
    RENDERER_API Result<void> destroyScene(Handel<Scene> scene);
    RENDERER_API Result<void> bindScene(Handel<Scene> scene);
    RENDERER_API Result<Handel<Scene>> getBoundScene();
    // Add to / remove from the bound scene. Adding a model that's already in it does nothing.
    RENDERER_API Result<void> addModelToScene(Handel<Model> model);
    RENDERER_API Result<void> removeModelFromScene(Handel<Model> model);
    //Angle is in radians.
    RENDERER_API Result<void> rotateModel(Handel<Model> model, float angle, glm::vec3 axis);
    RENDERER_API Result<void> scaleModel(Handel<Model> model, glm::vec3 factor);

    // Angles are in radians.
    RENDERER_API Result<void> setCameraPosition(glm::vec3 position);
    RENDERER_API Result<glm::vec3> getCameraPosition();

    RENDERER_API Result<void> moveCamera(glm::vec3 offset);
    RENDERER_API Result<void> moveCameraRelative(glm::vec3 offset);
    RENDERER_API Result<void> setCameraRotation(float yaw, float pitch);
    RENDERER_API Result<void> rotateCamera(float yawDelta, float pitchDelta);
    RENDERER_API Result<void> cameraLookAt(glm::vec3 target);
    RENDERER_API Result<glm::vec3> getCameraForward();
    RENDERER_API Result<glm::mat4> getViewMatrix();

    // Frames per second, averaged over the last half second.
    RENDERER_API Result<float> getFPS();
    // Seconds between the last two presented frames. Multiply movement by this to make it frame-rate independent.
    RENDERER_API Result<float> getDeltaTime();

};

#endif //VULKANRENDERER_RENDERER_H
