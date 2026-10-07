#pragma once
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include "VkBootstrap.h"
#include <vk_mem_alloc.h>

#include "../../include/renderer.h"
namespace Renderer {
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
    };
    struct Context {
        RenderConfig config;
        GLFWwindow* window = nullptr;
        VulkanContext vulkan;
    };
    Context* context();
}
