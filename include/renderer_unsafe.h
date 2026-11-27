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
    // Called every frame inside a render pass on the window (swapchain format, with the window's depth buffer), after
    // the tone-mapped scene is there: bind your own pipeline and state.
    // With a render target set, the window pass only contains what the callback draws. Pass nullptr to remove it.
    // Pipelines made with NRE's createPipeline declare dynamic cull mode: call vkCmdSetCullMode before drawing with them
    // here (the last pass leaves it at NONE).
    // Depth is reverse-Z: cleared to 0, nearer is greater, so depth-tested pipelines must use VK_COMPARE_OP_GREATER.
    RENDERER_API Result<void> registerFrameCallback(void (*frameCallback)(VkCommandBuffer commandBuffer));

    // Render to a texture: the scene is drawn into a width x height image instead of the window, and the
    // projection uses that size. The window is then cleared and only shows what the frame callback draws,
    // e.g. the texture in an ImGui panel. When the callback runs, the texture is in
    // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
    // With a render target set, pickInstance() takes render-target pixels (top-left origin), not window coordinates.
    // Calling it with the current size does nothing, so it can be called every frame with a panel's size.
    // A new size replaces the image (after waiting for the GPU): get the new view and re-register it with ImGui.
    RENDERER_API Result<void> setRenderTarget(uint32_t width, uint32_t height);
    // Back to drawing the scene straight to the window. Frees the texture.
    RENDERER_API Result<void> clearRenderTarget();
    // VK_NULL_HANDLE / {0, 0} while there's no render target. The format is the swapchain's: it holds the tone-mapped scene.
    // The image also has TRANSFER_SRC usage, so it can be copied out (e.g. for a screenshot).
    RENDERER_API VkImage getRenderTargetImage();
    RENDERER_API VkImageView getRenderTargetView();
    RENDERER_API VkExtent2D getRenderTargetExtent();
    RENDERER_API VkFormat getRenderTargetFormat();

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
