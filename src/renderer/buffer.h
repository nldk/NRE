#pragma once
#include <utility>
#include <vulkan/vulkan_core.h>

#include "renderer.h"
#include "vk_mem_alloc.h"

namespace NRE {
    struct Buffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = nullptr;
        VmaAllocationInfo allocationInfo{};
        // Empty buffer (VK_NULL_HANDLE); destroying it is a no-op.
        Buffer() = default;
        // allocationFlags: 0 for GPU-only memory, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT for memory the CPU writes to.
        Buffer(VkDeviceSize size,VkBufferUsageFlags usage, VmaAllocationCreateFlags allocationFlags);
        Result<void> createBuffer(VkDeviceSize size,VkBufferUsageFlags usage, VmaAllocationCreateFlags allocationFlags);
        Result<void> destroyBuffer();
        // Only valid for buffers created with a HOST_ACCESS allocation flag.
        Result<void> copyToBuffer(const void* data,VkDeviceSize offset, VkDeviceSize size);
        Result<VkMemoryPropertyFlags> getProperties();
        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;
        Buffer(Buffer&& o) noexcept
            : buffer(std::exchange(o.buffer, VK_NULL_HANDLE)),
              allocation(std::exchange(o.allocation, nullptr)),
              allocationInfo(o.allocationInfo) {}
        Buffer& operator=(Buffer&& o) noexcept {
            if (this != &o) {
                (void)destroyBuffer();
                buffer = std::exchange(o.buffer, VK_NULL_HANDLE);
                allocation = std::exchange(o.allocation, nullptr);
                allocationInfo = o.allocationInfo;
            }
            return *this;
        }
        ~Buffer();
    };
    Result<Handel<Buffer>> createBuffer(VkDeviceSize size,VkBufferUsageFlags usage, VmaAllocationCreateFlags allocationFlags);
}
