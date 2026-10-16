#pragma once
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include "VkBootstrap.h"
#include <vk_mem_alloc.h>

#include "resourceManagement.h"
#include "../../include/renderer.h"
namespace NRE {
    constexpr int max_frames_in_flight = 2;
    struct Frame {
        VkCommandPool pool;
        VkCommandBuffer cmd;
        VkFence in_flight;
        VkSemaphore image_acquired;
    };
    // Command buffer + fence for one-off GPU work outside the frame loop (e.g. copying staging buffers).
    struct UploadContext {
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
    };
    // D32_SFLOAT is supported as a depth attachment on all desktop GPUs.
    constexpr VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
    // Depth buffer, sized to the swapchain and recreated with it.
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
    };
    struct Window {
        GLFWwindow* window = nullptr;
        bool shouldClose = false;
        // Set by the framebuffer-size callback. Wayland never reports OUT_OF_DATE on resize, so this is how we notice.
        bool framebufferResized = false;
    };
    // Frame timing, updated once per presented frame. Times are from glfwGetTime(), in seconds.
    struct FrameTimer {
        double lastFrameTime = 0.0;
        double deltaTime = 0.0;
        // FPS is averaged over a window instead of 1/deltaTime, which jumps around too much to read.
        double windowStart = 0.0;
        uint32_t framesInWindow = 0;
        double fps = 0.0;
    };
    struct Context {
        RenderConfig config;
        Window window;
        FrameTimer timer;
        VulkanContext vulkan;
        ResourceManager manager;
        Handel<Scene> currentScene;
    };
    Context* context();
}
