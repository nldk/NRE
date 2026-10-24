#include "gpu/buffer.h"

#include <string>

#include "core/context.h"

namespace NRE {
    Result<void> Buffer::createBuffer(VkDeviceSize size,VkBufferUsageFlags usage, VmaAllocationCreateFlags allocationFlags) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer config not created. create with setRenderConfig.");
        VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bci.size = size;
        bci.usage = usage;
        VmaAllocationCreateInfo ainfo{};
        ainfo.usage = VMA_MEMORY_USAGE_AUTO;
        ainfo.flags = allocationFlags;
        if (vmaCreateBuffer(c->vulkan.allocator, &bci, &ainfo,&buffer,&allocation,&allocationInfo) != VK_SUCCESS) {
            buffer = VK_NULL_HANDLE;
            allocation = nullptr;
            if (ErrorHandeling::handleError(Error{ErrorCode::BufferCreationFailed,"Failed to create buffer of " + std::to_string(size) + " bytes."})) {
                return std::unexpected(Error{ErrorCode::BufferCreationFailed,"Failed to create buffer of " + std::to_string(size) + " bytes."});
            };
        }
        return {};
    }
    Result<void> Buffer::copyToBuffer(const void *data,VkDeviceSize offset, VkDeviceSize size) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer config not created. create with setRenderConfig.");
        if (vmaCopyMemoryToAllocation(c->vulkan.allocator,data,allocation,offset,size) != VK_SUCCESS) {
            if (ErrorHandeling::handleError(Error{ErrorCode::UploadFailed,"Failed to copy data into buffer."})) {
                return std::unexpected(Error{ErrorCode::UploadFailed,"Failed to copy data into buffer."});
            };
        }
        return {};
    }

    Result<VkMemoryPropertyFlags> Buffer::getProperties() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer config not created. create with setRenderConfig.");
        VkMemoryPropertyFlags props;
        vmaGetAllocationMemoryProperties(c->vulkan.allocator, allocation, &props);
        return props;
    }

    Result<void> Buffer::destroyBuffer() {
        if (buffer == VK_NULL_HANDLE) return {};
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer config not created. create with setRenderConfig.");
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
        NRE_REQUIRE_CONTEXT(c, "Renderer config not created. create with setRenderConfig.");
        Buffer buffer(size,usage,allocationFlags);
        if (buffer.buffer == VK_NULL_HANDLE) {
            return std::unexpected(Error{ErrorCode::BufferCreationFailed,"Failed to create buffer of " + std::to_string(size) + " bytes."});
        }
        return c->manager.create(std::move(buffer));
    }
}
