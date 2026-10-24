#ifndef VULKANRENDERER_RENDERER_UNSAFE_H
#define VULKANRENDERER_RENDERER_UNSAFE_H

#ifndef NRE_UNSAFE_API
#error "librenderer was built without NRE_UNSAFE_API. Configure with -DNRE_UNSAFE_API=ON (the *-unsafe CMake presets) to use renderer_unsafe.h."
#endif

#include <cstdint>
#include <vulkan/vulkan_core.h>

#include "renderer.h"

// Same types as GLFW's and VMA's own declarations, so the results can be passed straight to those libraries.
struct GLFWwindow;
struct VmaAllocator_T;

// Raw access to the renderer's internal handles, for things the normal API can't do (e.g. hooking up Dear ImGui).
// Everything here still belongs to the renderer: never destroy it yourself.
// Each function returns a null handle before init().
namespace NRE::Unsafe {
    // Valid until shutdown().
    RENDERER_API GLFWwindow* getWindow();
    RENDERER_API VkInstance getInstance();
    RENDERER_API VkSurfaceKHR getSurface();
    RENDERER_API VkPhysicalDevice getPhysicalDevice();
    RENDERER_API VkDevice getDevice();
    RENDERER_API VkQueue getGraphicsQueue();
    RENDERER_API uint32_t getGraphicsQueueFamily();
    RENDERER_API VmaAllocator_T* getAllocator();
    RENDERER_API Result<void> registerFrameCallback(void (*frameCallback)(VkCommandBuffer commandBuffer));

    // The sampler every texture uses.
    RENDERER_API VkSampler getSampler();

    // Replaced when the window is resized.
    RENDERER_API VkSwapchainKHR getSwapchain();
    RENDERER_API VkFormat getSwapchainFormat();
    RENDERER_API VkExtent2D getSwapchainExtent();
    RENDERER_API uint32_t getSwapchainImageCount();
    RENDERER_API VkFormat getDepthFormat();

}

#endif //VULKANRENDERER_RENDERER_UNSAFE_H
