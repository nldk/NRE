#include "gpu/buffer.h"

#include <string>

#include "core/context.h"

namespace NRE {
    Result<void> Buffer::createBuffer(VkDeviceSize size,VkBufferUsageFlags usage, VmaAllocationCreateFlags allocationFlags) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bci.size = size;
        bci.usage = usage;
        VmaAllocationCreateInfo ainfo{};
        ainfo.usage = VMA_MEMORY_USAGE_AUTO;
        ainfo.flags = allocationFlags;
        if (VkResult r = vmaCreateBuffer(c->vulkan.allocator, &bci, &ainfo,&buffer,&allocation,&allocationInfo); r != VK_SUCCESS) {
            buffer = VK_NULL_HANDLE;
            allocation = nullptr;
            auto error = Error{ErrorCode::BufferCreationFailed, "Failed to create a " + std::to_string(size) + "-byte buffer: "
                               + std::string(ErrorHandeling::vkResultName(r))};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
        }
        return {};
    }
    Result<void> Buffer::copyToBuffer(const void *data,VkDeviceSize offset, VkDeviceSize size) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        if (VkResult r = vmaCopyMemoryToAllocation(c->vulkan.allocator,data,allocation,offset,size); r != VK_SUCCESS) {
            auto error = Error{ErrorCode::UploadFailed, "Failed to copy " + std::to_string(size) + " bytes into a buffer at offset "
                               + std::to_string(offset) + ": " + std::string(ErrorHandeling::vkResultName(r))
                               + ". Only buffers created with a HOST_ACCESS allocation flag can be written from the CPU."};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
        }
        return {};
    }

    Result<VkMemoryPropertyFlags> Buffer::getProperties() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        VkMemoryPropertyFlags props;
        vmaGetAllocationMemoryProperties(c->vulkan.allocator, allocation, &props);
        return props;
    }

    Result<void> Buffer::destroyBuffer() {
        if (buffer == VK_NULL_HANDLE) return {};
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        vmaDestroyBuffer(c->vulkan.allocator,buffer,allocation);
        buffer = VK_NULL_HANDLE;
        allocation = nullptr;
        return {};
    }

    Buffer::Buffer(VkDeviceSize size,VkBufferUsageFlags usage, VmaAllocationCreateFlags allocationFlags) {
        (void)createBuffer(size,usage,allocationFlags);
    }

    Buffer::~Buffer() {
        (void)destroyBuffer();
    }

    Result<Handel<Buffer> > createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaAllocationCreateFlags allocationFlags) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        Buffer buffer(size,usage,allocationFlags);
        if (buffer.buffer == VK_NULL_HANDLE) {
            return std::unexpected(Error{ErrorCode::BufferCreationFailed,"Failed to create buffer of " + std::to_string(size) + " bytes."});
        }
        return c->manager.create(std::move(buffer));
    }
}
