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
    struct VulkanContext {
        vkb::Instance instance;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        vkb::Device device;
        VmaAllocator allocator = nullptr;
        vkb::Swapchain swapchain;
        vkb::PhysicalDevice physicalDevice;
        Frame frames[max_frames_in_flight];
        std::vector<VkSemaphore> renderDone;
        uint32_t currentFrame = 0;
        std::vector<VkImage> images;
        std::vector<VkImageView> views;
        VkQueue graphicsQueue;
        uint32_t currentImageIndex = 0;
        uint32_t graphicsQueueFamily = 0;
        Handel<Pipeline> pipelineBound;
    };
    struct Window {
        GLFWwindow* window = nullptr;
        bool shouldClose = false;
    };
    struct Context {
        RenderConfig config;
        Window window;
        VulkanContext vulkan;
        ResourceManager manager;
    };
    Context* context();
}
