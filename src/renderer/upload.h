#pragma once
#include <functional>
#include <vulkan/vulkan_core.h>

#include "renderer.h"

namespace NRE {
    // Records commands with `record`, submits them on the graphics queue and blocks until the GPU is done.
    // Meant for load-time work like staging-buffer copies, not for per-frame rendering.
    Result<void> immediateSubmit(const std::function<void(VkCommandBuffer)>& record);
}
