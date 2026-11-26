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
}
