#pragma once
#include <vulkan/vulkan_core.h>

#include "renderer.h"
#include "vk_mem_alloc.h"

namespace NRE {
    struct Buffer {
        VkBuffer buffer;
        VmaAllocation allocation;
        VmaAllocationInfo allocationInfo;
        Buffer();
        Result<void> createBuffer(uint32_t size,VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
        Result<void> destroyBuffer();
        Result<void> copyToBuffer(char* data,uint32_t offset, uint32_t size);
    };
}
