#pragma once
#include <vulkan/vulkan_core.h>

#include "gpu/image.h"
#include "renderer.h"

namespace NRE {
    // The scene's HDR colour target (hdrFormat, at the scene's size) and the full-screen pass that tone maps it
    // into the swapchain format at the end of the frame. Lives in the Context; shutdown() calls destroy().
    class ToneMapPass {
        Image hdr;
        Handel<Pipeline> pipeline;   // in the resource manager, so destroyAll() frees it
        VkDescriptorSet set = VK_NULL_HANDLE;   // set 1 (the material layout): binding 0 = hdr
    public:
        // The pipeline (for the swapchain format) and the descriptor set. No image yet: call resize().
        Result<void> create();
        // (Re)creates the HDR image at this size; nothing happens if it already is. Rewrites the descriptor set,
        // so the GPU must not be using it: call it after vkDeviceWaitIdle. On failure the old image stays.
        Result<void> resize(VkExtent2D extent);
        // Inside a render pass on the output (swapchain format, with a depth attachment), with the HDR image in
        // SHADER_READ_ONLY_OPTIMAL. passthrough2D copies instead of tone mapping (Render_2D).
        void record(VkCommandBuffer commandBuffer, float exposure, bool passthrough2D);
        void destroy();
        VkImageView hdrView() const { return hdr.getView(); }
        VkImage hdrImage() const { return hdr.getImage(); }
        VkExtent2D hdrExtent() const { return hdr.getExtent(); }
    };
}
