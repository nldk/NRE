// Only compiled when CMake is configured with NRE_UNSAFE_API=ON; otherwise none of this is in librenderer.so.
#include "renderer_unsafe.h"

#include "core/context.h"

namespace NRE::Unsafe {
    GLFWwindow* getWindow() {
        auto c = context();
        return c ? c->window.window : nullptr;
    }

    VkInstance getInstance() {
        auto c = context();
        return c ? c->vulkan.instance.instance : VK_NULL_HANDLE;
    }

    VkSurfaceKHR getSurface() {
        auto c = context();
        return c ? c->vulkan.surface : VK_NULL_HANDLE;
    }

    VkPhysicalDevice getPhysicalDevice() {
        auto c = context();
        return c ? c->vulkan.physicalDevice.physical_device : VK_NULL_HANDLE;
    }

    VkDevice getDevice() {
        auto c = context();
        return c ? c->vulkan.device.device : VK_NULL_HANDLE;
    }

    VkQueue getGraphicsQueue() {
        auto c = context();
        return c ? c->vulkan.graphicsQueue : VK_NULL_HANDLE;
    }

    uint32_t getGraphicsQueueFamily() {
        auto c = context();
        return c ? c->vulkan.graphicsQueueFamily : 0;
    }

    VmaAllocator_T* getAllocator() {
        auto c = context();
        return c ? c->vulkan.allocator : nullptr;
    }

    VkSampler getSampler() {
        auto c = context();
        return c ? c->vulkan.sampler : VK_NULL_HANDLE;
    }

    VkSwapchainKHR getSwapchain() {
        auto c = context();
        return c ? c->vulkan.swapchain.swapchain : VK_NULL_HANDLE;
    }

    VkFormat getSwapchainFormat() {
        auto c = context();
        return c ? c->vulkan.swapchain.image_format : VK_FORMAT_UNDEFINED;
    }

    VkExtent2D getSwapchainExtent() {
        auto c = context();
        return c ? c->vulkan.swapchain.extent : VkExtent2D{0, 0};
    }

    uint32_t getSwapchainImageCount() {
        auto c = context();
        return c ? static_cast<uint32_t>(c->vulkan.images.size()) : 0;
    }

    VkFormat getDepthFormat() {
        return depthFormat;
    }
    Result<void> registerFrameCallback(void (*frameCallback)(VkCommandBuffer commandBuffer)) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        c->unsafeContext.frameCallback = frameCallback;
        return {};
    }
}
