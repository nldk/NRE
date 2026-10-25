#ifndef VULKANRENDERER_RENDERER_H
#define VULKANRENDERER_RENDERER_H
#include <expected>
#include <ostream>
#include <renderer_export.h>
#include <string>
#include <string_view>
#include <vector>

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
    enum class MouseButton {
        Left,
        Right,
        Middle,
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
        ErrorCreatingTexture,
        ImageCreationFailed,
        ImageViewCreationFailed,
        IndexOutOfRange,
        ImpossibleTypeForPerFrameBuffer,
        BufferWithSizeNull,
        SizeOfBufferToBig,
        UnknownError,
    };
    struct WindowConfig {
        int width,height;
        std::string title;
        bool resizable;
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
            case ErrorCode::IndexOutOfRange:      return "IndexOutOfRange";
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
            case ErrorCode::ErrorCreatingTexture:return "ErrorCreatingTexture";
            case ErrorCode::ImageCreationFailed: return "ImageCreationFailed";
            case ErrorCode::ImageViewCreationFailed: return "ImageViewCreationFailed";
            case ErrorCode::ImpossibleTypeForPerFrameBuffer: return "ImpossibleTypeForPerFrameBuffer";
            case ErrorCode::BufferWithSizeNull: return "BufferWithSizeNull";
            case ErrorCode::SizeOfBufferToBig: return "SizeOfBufferToBig";
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
    struct Instance;
    struct Light;
    class Scene;
    class Texture;
    struct Material;

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
    // A Model is the loaded file (meshes, GPU buffers, materials) and isn't drawn by itself.
    // Draw it through instances: each one is a copy with its own transform, and they all share the model's data.
    RENDERER_API Result<Handel<Model>> createModel(std::string modelPath);
    // Instances of a destroyed model stay valid but draw nothing.
    RENDERER_API Result<void> destroyModel(Handel<Model> model);
    RENDERER_API Result<Handel<Instance>> createInstance(Handel<Model> model);
    // Also removes it from every scene it's in.
    RENDERER_API Result<void> destroyInstance(Handel<Instance> instance);
    RENDERER_API Result<void> setInstanceTransform(Handel<Instance> instance, const Transform& transform);
    RENDERER_API Result<Transform> getInstanceTransform(Handel<Instance> instance);
    RENDERER_API Result<glm::mat4> getInstanceMatrix(Handel<Instance> instance);
    RENDERER_API Result<void> translateInstance(Handel<Instance> instance, glm::vec3 offset);
    // Angle is in radians.
    RENDERER_API Result<void> rotateInstance(Handel<Instance> instance, float angle, glm::vec3 axis);
    RENDERER_API Result<void> scaleInstance(Handel<Instance> instance, glm::vec3 factor);
    // A hidden instance stays in its scenes but isn't drawn.
    RENDERER_API Result<void> setVisibility(Handel<Instance> instance, bool visible);
    RENDERER_API Result<Handel<Scene>> createScene();
    RENDERER_API Result<void> destroyScene(Handel<Scene> scene);
    RENDERER_API Result<void> bindScene(Handel<Scene> scene);
    RENDERER_API Result<Handel<Scene>> getBoundScene();
    // Add to / remove from the bound scene. Adding an instance that's already in it does nothing.
    RENDERER_API Result<void> addInstanceToScene(Handel<Instance> instance);
    RENDERER_API Result<void> removeInstanceFromScene(Handel<Instance> instance);

    // Textures and materials. Models loaded from glTF make their own; these are for adding or swapping them yourself.
    // Materials belong to the model, so changing one changes every instance of that model.
    // Loads an image file (PNG, JPEG, ...) as a color (sRGB) texture.
    RENDERER_API Result<Handel<Texture>> createTexture(std::string path);
    // Give the materials that use this texture another one first: they keep pointing at it.
    RENDERER_API Result<void> destroyTexture(Handel<Texture> texture);
    // An empty baseTexture uses the default texture. metallicTexture and normalTexture are stored but not used yet.
    RENDERER_API Result<Handel<Material>> createMaterial(Handel<Texture> baseTexture = {},
                                                         Handel<Texture> metallicTexture = {},
                                                         Handel<Texture> normalTexture = {});
    // Models still using it fall back to the default material.
    RENDERER_API Result<void> destroyMaterial(Handel<Material> material);
    // An empty texture uses the default texture. Waits for the GPU to finish the frames in flight, so don't call it every frame.
    RENDERER_API Result<void> setMaterialBaseTexture(Handel<Material> material, Handel<Texture> texture);
    // The model's material slots, in the file's material order. Empty for a model without materials (like OBJ).
    RENDERER_API Result<std::vector<Handel<Material>>> getModelMaterials(Handel<Model> model);
    // Makes every mesh of the model use material, replacing all of its slots.
    RENDERER_API Result<void> setModelMaterial(Handel<Model> model, Handel<Material> material);
    // Replaces slot index (an index into getModelMaterials), so only the meshes using that slot change.
    RENDERER_API Result<void> replaceModelMaterial(Handel<Model> model, uint32_t index, Handel<Material> material);

    // Lights. Like instances: create one, then add it to the bound scene. Intensity is in arbitrary units:
    // a directional light around 3 looks like plain diffuse lighting, a point light falls off with 1/distance².
    // direction is the way the light travels (e.g. {0, -1, 0} shines straight down).
    RENDERER_API Result<Handel<Light>> createDirectionalLight(glm::vec3 direction, glm::vec3 color = glm::vec3(1.0f), float intensity = 3.0f);
    RENDERER_API Result<Handel<Light>> createPointLight(glm::vec3 position, glm::vec3 color = glm::vec3(1.0f), float intensity = 10.0f);
    // Also removes it from every scene it's in.
    RENDERER_API Result<void> destroyLight(Handel<Light> light);
    // Add to / remove from the bound scene. Adding a light that's already in it does nothing.
    RENDERER_API Result<void> addLightToScene(Handel<Light> light);
    RENDERER_API Result<void> removeLightFromScene(Handel<Light> light);
    // Only used by point lights.
    RENDERER_API Result<void> setLightPosition(Handel<Light> light, glm::vec3 position);
    // Only used by directional lights.
    RENDERER_API Result<void> setLightDirection(Handel<Light> light, glm::vec3 direction);
    RENDERER_API Result<void> setLightColor(Handel<Light> light, glm::vec3 color, float intensity);

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

    // Mouse input. The deltas cover the time between the last two pollEvents calls.
    RENDERER_API Result<bool> isMouseButtonDown(MouseButton button);
    // How far the cursor moved, in pixels. +x is right, +y is down.
    RENDERER_API Result<glm::vec2> getMouseDelta();
    // Scroll wheel steps. Positive means scrolling up / away from you.
    RENDERER_API Result<float> getScrollDelta();

};

#endif //VULKANRENDERER_RENDERER_H
