#pragma once
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include "VkBootstrap.h"
#include <vk_mem_alloc.h>
#include <vector>
#include "core/resourceManagement.h"
#include "renderer.h"
namespace NRE {
    constexpr int max_frames_in_flight = 2;
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
        Handel<Texture> defaultTexture;
        Handel<Material> defaultMaterial;
    };
    struct Window {
        GLFWwindow* window = nullptr;
        bool shouldClose = false;
        bool framebufferResized = false;

        glm::dvec2 cursor{0.0};
        glm::vec2 cursorDelta{0.0f};
        float scrollDelta = 0.0f;
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
}
