#pragma once
#include <vulkan/vulkan_core.h>

#include "renderer.h"
#include "vk_mem_alloc.h"

namespace NRE {
    struct Buffer {
        VkBuffer buffer;
        VmaAllocation allocation;
        Buffer();
    };
}
