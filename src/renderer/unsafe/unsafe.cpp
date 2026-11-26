// Only compiled when CMake is configured with NRE_UNSAFE_API=ON; otherwise none of this is in librenderer.so.
#include "renderer_unsafe.h"

#include <string>

#include "core/context.h"
#include "core/errorHandling.h"

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

    Result<void> setRenderTarget(uint32_t width, uint32_t height) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
#ifndef NDEBUG
        if (c->vulkan.device.device == VK_NULL_HANDLE) {
            auto error = Error{ErrorCode::NotInitialized, "Unsafe::setRenderTarget(): called before init(); "
                               "the render target needs the device and the swapchain format."};
            ErrorHandeling::handleError(error);
            return std::unexpected(error);
        }
        if (width == 0 || height == 0) {
            auto error = Error{ErrorCode::BufferWithSizeNull, "Unsafe::setRenderTarget(): " + std::to_string(width) + "x"
                               + std::to_string(height) + " has no pixels. Use clearRenderTarget() to draw to the window again."};
            ErrorHandeling::handleError(error);
            return std::unexpected(error);
        }
#endif
        UnsafeContext& u = c->unsafeContext;
        VkExtent2D current = u.targetColor.getExtent();
        if (u.targetColor.getImage() != VK_NULL_HANDLE && current.width == width && current.height == height) return {};

        // Frames in flight may still be drawing into, or sampling, the old images.
        vkDeviceWaitIdle(c->vulkan.device);
        // Same color and depth formats as the window: the tone-map pass writes the scene into it like into the window.
        Image color, depth;
        // TRANSFER_SRC so it can also be copied out (screenshots, thumbnails).
        auto created = color.createImage(width, height, c->vulkan.swapchain.image_format,
                                         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 1);
        if (created && color.getImage() != VK_NULL_HANDLE) {
            created = depth.createImage(width, height, depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 1);
        }
        // Under RenderErrorPolicy::Ignore a failed createImage returns success with no image; keep the old target then.
        if (!created || color.getImage() == VK_NULL_HANDLE || depth.getImage() == VK_NULL_HANDLE) return created;
        // The scene is drawn into the HDR target first, at the same size, then tone mapped into this one.
        if (auto hdr = c->vulkan.toneMap.resize({width, height}); !hdr) return hdr;

        u.targetColor = std::move(color);   // the move assignments free the old images
        u.targetDepth = std::move(depth);
        updateProjection(c);
        return {};
    }

    Result<void> clearRenderTarget() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        UnsafeContext& u = c->unsafeContext;
        if (u.targetColor.getImage() == VK_NULL_HANDLE) return {};
        vkDeviceWaitIdle(c->vulkan.device);   // the last frames may still use the images
        (void)u.targetColor.destroyImage();
        (void)u.targetDepth.destroyImage();
        updateProjection(c);
        return c->vulkan.toneMap.resize(sceneExtent(c));   // back to the window's size
    }

    VkImage getRenderTargetImage() {
        auto c = context();
        return c ? c->unsafeContext.targetColor.getImage() : VK_NULL_HANDLE;
    }

    VkImageView getRenderTargetView() {
        auto c = context();
        return c ? c->unsafeContext.targetColor.getView() : VK_NULL_HANDLE;
    }

    VkExtent2D getRenderTargetExtent() {
        auto c = context();
        return c && c->unsafeContext.targetColor.getImage() != VK_NULL_HANDLE ? c->unsafeContext.targetColor.getExtent() : VkExtent2D{0, 0};
    }

    VkFormat getRenderTargetFormat() {
        auto c = context();
        return c && c->unsafeContext.targetColor.getImage() != VK_NULL_HANDLE ? c->unsafeContext.targetColor.getFormat() : VK_FORMAT_UNDEFINED;
    }
}
