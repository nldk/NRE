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
    // Physical keys, named after the US layout (Key::W is the key where W is on a US keyboard, whatever the layout).
    // The values are GLFW's key codes.
    enum class Key {
        Space = 32, Apostrophe = 39, Comma = 44, Minus = 45, Period = 46, Slash = 47,
        Zero = 48, One, Two, Three, Four, Five, Six, Seven, Eight, Nine,
        Semicolon = 59, Equal = 61,
        A = 65, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        LeftBracket = 91, Backslash = 92, RightBracket = 93, GraveAccent = 96,
        Escape = 256, Enter, Tab, Backspace, Insert, Delete, Right, Left, Down, Up, PageUp, PageDown, Home, End,
        CapsLock = 280, ScrollLock, NumLock, PrintScreen, Pause,
        F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
        Keypad0 = 320, Keypad1, Keypad2, Keypad3, Keypad4, Keypad5, Keypad6, Keypad7, Keypad8, Keypad9,
        KeypadDecimal, KeypadDivide, KeypadMultiply, KeypadSubtract, KeypadAdd, KeypadEnter, KeypadEqual,
        LeftShift = 340, LeftControl, LeftAlt, LeftSuper, RightShift, RightControl, RightAlt, RightSuper, Menu,
    };
    enum class CursorMode {
        Normal,
        // Invisible while it's over the window, but moves and leaves the window as usual.
        Hidden,
        // Hidden and kept in the window, with unlimited movement: for mouse-look cameras.
        // getMouseDelta keeps working; getMousePosition is no longer a point in the window.
        Locked,
    };
    enum class RenderErrorPolicy {
        Ignore,
        Return,
        Panic,
    };
    enum class ApiMode {
        SceneMode,
        FunctionDrawMode,
    };
    // A scene's lighting mode (setSceneLighting).
    enum class SceneLighting {
        // Default; today's behaviour. Sky: the scene's cubemap, else the sky colour. One sharp shadow map over a short
        // distance, no haze, exposure is exactly setExposure.
        Small,
        // Procedural sky computed from the sun, sky-lit ambient and reflections, cascaded shadows reaching far, haze,
        // and an automatic exposure that follows the sun's height (setExposure multiplies on top).
        Landscape,
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
        DescriptorCreationFailed,
        ShaderCreationFailed,
        UnknownError,
        // Added after 0.1.0: new codes go at the end so the existing values don't change.
        LibraryLoadFailed,
        InvalidArgument,
    };
    struct WindowConfig {
        int width,height;
        std::string title;
        bool resizable;
        bool showFpsInTitle = true;
    };
    struct RenderConfig {
        RenderMode mode;
        ApiMode apiMode;
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
            case ErrorCode::DescriptorCreationFailed: return "DescriptorCreationFailed";
            case ErrorCode::ShaderCreationFailed: return "ShaderCreationFailed";
            case ErrorCode::LibraryLoadFailed: return "LibraryLoadFailed";
            case ErrorCode::InvalidArgument: return "InvalidArgument";
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
    class Cubemap;

    RENDERER_API Result<void> setRenderConfig(RenderConfig config);
    RENDERER_API Result<void> init();
    RENDERER_API Result<void> shutdown();
    // SceneMode: draws the bound scene and (unless manualFramePresentationAndSubmission) submits and presents it.
    // FunctionDrawMode: starts the frame, raylib style: renderFrame(), then drawModel()/drawInstance() calls, then endFrame().
    // The camera and lights come from the bound scene (its instances aren't drawn), and so does the pipeline bound now.
    RENDERER_API Result<void> renderFrame();
    // FunctionDrawMode only: ends the frame renderFrame() started and (unless manualFramePresentationAndSubmission)
    // submits and presents it.
    RENDERER_API Result<void> endFrame();
    RENDERER_API Result<void> presentFrame();
    RENDERER_API Result<void> submitFrame();
    RENDERER_API Result<bool> windowShouldClose();
    RENDERER_API Result<void> pollEvents();
    RENDERER_API Result<Handel<Shader>> createShader(std::string shaderPath,ShaderType type);
    RENDERER_API Result<void> destroyShader(Handel<Shader> shader);
    // Shaders for it: vertex shaders #include "nre/frame.glsl" (nreFrame.viewProj, ...), fragment shaders
    // #include "nre/lighting.glsl" and call nreMaterialSurface + nreShade (see example/shaders). nre_compile_shaders()
    // puts the includes on the path. Back faces are culled unless the mesh's material is double-sided.
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
    // FunctionDrawMode only, between renderFrame() and endFrame(). Draws the model once with this transform,
    // no instance needed. Do nothing while renderFrame() skipped the frame (e.g. the window is being resized).
    RENDERER_API Result<void> drawModel(Handel<Model> model, const Transform& transform = {});
    // Uses the instance's transform. A hidden instance isn't drawn.
    RENDERER_API Result<void> drawInstance(Handel<Instance> instance);
    RENDERER_API Result<Handel<Scene>> createScene();
    RENDERER_API Result<void> destroyScene(Handel<Scene> scene);
    RENDERER_API Result<void> bindScene(Handel<Scene> scene);
    RENDERER_API Result<Handel<Scene>> getBoundScene();
    // Add to / remove from the bound scene. Adding an instance that's already in it does nothing.
    RENDERER_API Result<void> addInstanceToScene(Handel<Instance> instance);
    RENDERER_API Result<void> removeInstanceFromScene(Handel<Instance> instance);

    // Textures and materials. Models loaded from glTF make their own; these are for adding or swapping them yourself.
    // Materials belong to the model, so changing one changes every instance of that model.
    // Loads an image file (PNG, JPEG, ...). srgb = true for colour images (base colour), decoded from sRGB when sampled.
    // Data maps hold plain numbers and need srgb = false: normal maps and metallic-roughness maps.
    RENDERER_API Result<Handel<Texture>> createTexture(std::string path, bool srgb = true);
    // Give the materials that use this texture another one first: they keep pointing at it.
    RENDERER_API Result<void> destroyTexture(Handel<Texture> texture);
    // Textures as glTF uses them: baseTexture is the colour (sRGB), metallicTexture holds roughness in green and
    // metallic in blue, normalTexture is a tangent-space normal map (+Y up); load both of those with srgb = false.
    // An empty baseTexture is white, an empty normalTexture flat. Without a metallicTexture the material isn't
    // metal and has roughness 1; with one, the texture's values are used as they are.
    RENDERER_API Result<Handel<Material>> createMaterial(Handel<Texture> baseTexture = {},
                                                         Handel<Texture> metallicTexture = {},
                                                         Handel<Texture> normalTexture = {});
    // Models still using it fall back to the default material (white). Waits for the GPU to finish the frames in flight.
    RENDERER_API Result<void> destroyMaterial(Handel<Material> material);
    // An empty texture means white. Waits for the GPU to finish the frames in flight, so don't call it every frame.
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

    // The sun: one directional light per scene, kept apart from the scene's other lights so it can drive the
    // sky and shadows. Same direction and intensity conventions as createDirectionalLight.
    // A scene starts without one (intensity 0). nreShade lights with it; shaders can also read nreFrame.sunDirection
    // (the way the light travels), nreFrame.sunColor (rgb colour, w intensity) and nreFrame.sunRadiance (the light
    // that reaches objects) from nre/frame.glsl. In Landscape the sun also makes the sky: its light is tinted by the
    // atmosphere (warm and dim near the horizon, gone once it has set), and the intensity scales the sky's brightness
    // with it (3, the default, is the standard look).
    RENDERER_API Result<void> setSun(glm::vec3 direction, glm::vec3 color = glm::vec3(1.0f), float intensity = 3.0f);
    // Keeps the color and intensity, e.g. for a day/night cycle.
    RENDERER_API Result<void> setSunDirection(glm::vec3 direction);
    RENDERER_API Result<glm::vec3> getSunDirection();
    // Switches the bound scene's mode and resets shadow distance, haze and the sky to that mode's defaults:
    //   Small:     shadows 20 m, haze off,                    sky = cubemap if set, else the sky colour
    //   Landscape: shadows 500 m, haze visibility 8000 m,     sky = procedural (or the cubemap if set)
    // The sun, lights, exposure and sky colour are kept.
    RENDERER_API Result<void> setSceneLighting(SceneLighting lighting);
    RENDERER_API Result<SceneLighting> getSceneLighting();
    // How far from the camera the bound scene's sun casts shadows, in metres. 0 turns sun shadows off; must not be
    // negative. Default 20 m (Small) / 500 m (Landscape); setSceneLighting resets it. The number of cascades follows from it (up to 4); shorter
    // distances give sharper shadows. Only the sun casts shadows, and only in Render_3D.
    RENDERER_API Result<void> setShadowDistance(float meters);
    // Whether an instance casts sun shadows (it still receives them). Default true; e.g. off for grass, for speed.
    // drawModel() always casts.
    RENDERER_API Result<void> setCastsShadows(Handel<Instance> instance, bool castsShadows);

    // Tone mapping. Shaders write linear light (values above 1 are fine, and must not tone map themselves);
    // at the end of the frame the renderer multiplies it by the exposure and maps it to the screen with the
    // ACES curve. Render_2D isn't tone mapped: what the shaders write is what's shown.
    // The bound scene's exposure: 2 is twice as bright, 0.5 half. Must be greater than 0. Default 1. In Landscape it
    // multiplies an automatic exposure that follows the sun's height (1 from 30 degrees up, rising smoothly to 5 at
    // night), so noon, golden hour and dusk all read well; dusk still comes out darker and warmer than noon.
    RENDERER_API Result<void> setExposure(float exposure);
    // The bound scene's background where nothing is drawn and there's no skybox (in Small; Landscape shows the
    // procedural sky), as the colour it shows on screen (linear 0..1, at any exposure). Default {0.1, 0.3, 0.6}.
    // A skybox likewise looks like its images at any exposure.
    RENDERER_API Result<void> setSkyColor(glm::vec3 color);

    // Cubemaps and the skybox. A cubemap is six square images of the same size, one per side of a cube around
    // the camera, loaded as color (sRGB). The faces are in the OpenGL order, the one most skybox downloads name
    // their files after: right (+X), left (-X), top (+Y), bottom (-Y), front (+Z), back (-Z).
    RENDERER_API Result<Handel<Cubemap>> createCubemap(std::string right, std::string left, std::string top,
                                                       std::string bottom, std::string front, std::string back);
    // Waits for the GPU to finish the frames in flight. Scenes using it go back to having no skybox.
    RENDERER_API Result<void> destroyCubemap(Handel<Cubemap> cubemap);
    // The bound scene's sky, drawn behind everything (3D only), as authored, in either lighting mode. An empty handle
    // removes it: the mode's own sky shows again (Small: the sky colour; Landscape: the procedural sky).
    // Shaders can also sample it, e.g. for reflections: nreSky in nre/frame.glsl (set 0, binding 2), with
    // nreFrame.skyParams.x telling what it holds: 1 this cubemap (display values), 2 the procedural sky (linear light,
    // times nreFrame.skyParams.z), 0 neither (a black cubemap).
    RENDERER_API Result<void> setSkybox(Handel<Cubemap> cubemap);

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
    // Pressed / released: the button went down / up during the last pollEvents call, so it's true for one frame.
    // A click shorter than a frame shows up as both pressed and released, while isMouseButtonDown stays false.
    RENDERER_API Result<bool> isMouseButtonPressed(MouseButton button);
    RENDERER_API Result<bool> isMouseButtonReleased(MouseButton button);
    // Cursor position in pixels from the window's top-left corner, as of the last pollEvents call.
    RENDERER_API Result<glm::vec2> getMousePosition();
    RENDERER_API Result<void> setCursorMode(CursorMode mode);

    // Keyboard input. Same rules as the mouse buttons: pressed / released are true for the one frame
    // after the pollEvents call that saw it happen. Holding a key doesn't repeat isKeyPressed.
    RENDERER_API Result<bool> isKeyDown(Key key);
    RENDERER_API Result<bool> isKeyPressed(Key key);
    RENDERER_API Result<bool> isKeyReleased(Key key);

};

#endif //VULKANRENDERER_RENDERER_H
