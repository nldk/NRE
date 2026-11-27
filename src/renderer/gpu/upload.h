#pragma once
#include <functional>
#include <vulkan/vulkan_core.h>

#include "renderer.h"

namespace NRE {
    // Records commands with `record`, submits them on the graphics queue and blocks until the GPU is done.
    // Meant for load-time work like staging-buffer copies, not for per-frame rendering.
    Result<void> immediateSubmit(const std::function<void(VkCommandBuffer)>& record);

    // Records a barrier that moves mip 0 of the first layerCount layers of img (6 for a cubemap) from one layout
    // to another. Defined in core/renderer.cpp.
    void transition(VkCommandBuffer cmd, VkImage img, VkImageLayout from, VkImageLayout to,
                    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT, uint32_t layerCount = 1);

    // Records one image barrier over mips baseMip .. baseMip + mipCount - 1 and the first `layers` layers of a colour
    // image, with exact stages and accesses (unlike transition(), which waits on everything).
    void imageBarrier(VkCommandBuffer cmd, VkImage image, uint32_t baseMip, uint32_t mipCount, uint32_t layers,
                      VkImageLayout from, VkImageLayout to,
                      VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
                      VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess);

    // Records the mip chain of a cubemap whose mip 0 a compute shader just wrote (in GENERAL): each level a linear 2x2
    // average of the one above, by blits. Afterwards every level is SHADER_READ_ONLY_OPTIMAL and visible to fragment
    // and compute shaders. The old contents of levels 1.. are discarded. size: mip 0's width.
    void recordCubemapMips(VkCommandBuffer cmd, VkImage image, uint32_t size, uint32_t mipLevels);
}
