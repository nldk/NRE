#pragma once
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include "VkBootstrap.h"
#include <vk_mem_alloc.h>
#include <bitset>
#include <vector>
#include "core/resourceManagement.h"
#include "renderer.h"
#include "core/frames.h"
#include "gpu/image.h"
#include "core/frameRecorder.h"
#include "post/toneMap.h"
namespace NRE {
    struct Frame {
        VkCommandPool pool;
        VkCommandBuffer cmd;
        VkFence in_flight;
        VkSemaphore image_acquired;
    };
    struct UploadContext {
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
    };
    constexpr VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
    struct DepthImage {
        VkImage image = VK_NULL_HANDLE;
        VmaAllocation allocation = nullptr;
        VkImageView view = VK_NULL_HANDLE;
    };
    struct VulkanContext {
        vkb::Instance instance;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        vkb::Device device;
        VmaAllocator allocator = nullptr;
        vkb::Swapchain swapchain;
        vkb::PhysicalDevice physicalDevice;
        Frame frames[max_frames_in_flight];
        UploadContext upload;
        std::vector<VkSemaphore> renderDone;
        uint32_t currentFrame = 0;
        std::vector<VkImage> images;
        std::vector<VkImageView> views;
        DepthImage depth;
        VkQueue graphicsQueue;
        uint32_t currentImageIndex = 0;
        uint32_t graphicsQueueFamily = 0;
        Handel<Pipeline> pipelineBound;
        glm::mat4 projection;
        VkSampler sampler = VK_NULL_HANDLE;
        DescriptorSetPool   descriptorPool;
        std::vector<DescriptorSetLayout> descriptorLayouts;
        // The magenta/black checker: shown where a model's base colour image failed to load.
        Handel<Texture> defaultTexture;
        // 1x1 material defaults: white (base colour, metallic-roughness) and a flat normal (128, 128, 255).
        Handel<Texture> whiteTexture;
        Handel<Texture> flatNormalTexture;
        // For meshes without a material (OBJ): white, non-metal, roughness 1.
        Handel<Material> defaultMaterial;
        // 1x1 black faces: what set 0 binding 2 shows while a scene has no skybox.
        Handel<Cubemap> defaultCubemap;
        Handel<Pipeline> skyboxPipeline;
        // The HDR scene target and the pass that tone maps it to the window / render target.
        ToneMapPass toneMap;
        // What renderFrame() set up the frame with (the bound scene, whose frame data it wrote, and pipeline),
        // so the frame recorded at endFrame() uses them even if others are bound in between.
        Handel<Scene> frameScene;
        Handel<Pipeline> framePipeline;
        // This frame's draws: FunctionDrawMode queues them between renderFrame() and endFrame(), SceneMode fills it
        // from the scene. Cleared every frame; keeps its capacity.
        std::vector<QueuedDraw> drawQueue;
        // FunctionDrawMode: true between renderFrame() and endFrame().
        bool isDrawing = false;
        // FunctionDrawMode: false when renderFrame() skipped the frame (swapchain out of date, no pipeline under
        // RenderErrorPolicy::Ignore), so the command buffer isn't recording and draw calls do nothing.
        bool frameRecording = false;
    };
    struct Window {
        GLFWwindow* window = nullptr;
        bool shouldClose = false;
        bool framebufferResized = false;

        glm::dvec2 cursor{0.0};
        glm::vec2 cursorDelta{0.0f};
        float scrollDelta = 0.0f;
        // What went down / up during the last pollEvents call. Set by keyCallback / mouseButtonCallback,
        // cleared at the start of the next pollEvents.
        std::bitset<GLFW_KEY_LAST + 1> keysPressed, keysReleased;
        std::bitset<GLFW_MOUSE_BUTTON_LAST + 1> buttonsPressed, buttonsReleased;
    };

    struct FrameTimer {
        double lastFrameTime = 0.0;
        double deltaTime = 0.0;
        double windowStart = 0.0;
        uint32_t framesInWindow = 0;
        double fps = 0.0;
    };
#ifdef NRE_UNSAFE_API
    struct UnsafeContext {
          void (*frameCallback)(VkCommandBuffer) = nullptr;
          // Render target (Unsafe::setRenderTarget): while targetColor has an image, the scene is drawn into
          // these instead of the window. Destroyed in shutdown(), before the allocator.
          Image targetColor;
          Image targetDepth;
    };
#endif
    struct Context {
        RenderConfig config;
        Window window;
        FrameTimer timer;
        VulkanContext vulkan;
        ResourceManager manager;
        Handel<Scene> currentScene;
        #ifdef NRE_UNSAFE_API
        UnsafeContext unsafeContext;
        #endif
    };
    Context* context();
    // Rebuilds c->vulkan.projection for the size the scene is drawn at (the render target's, if there is one).
    // Defined in core/renderer.cpp.
    void updateProjection(Context* c);
    // The size the scene is drawn at: the unsafe render target's if one is set, otherwise the window's.
    // Defined in core/renderer.cpp.
    VkExtent2D sceneExtent(Context* c);
}
