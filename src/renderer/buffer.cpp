#include "buffer.h"

#include "context.h"

namespace NRE {
    Result<void> Buffer::createBuffer(uint32_t size,VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) {
        auto c = context();
        if (!c) {
            if (ErrorHandeling::handleError(Error{ErrorCode::NotInitialized,"Renderer config not created. create with setRenderConfig."})) {
                return std::unexpected(Error{ErrorCode::NotInitialized,"Renderer config not created. create with setRenderConfig."});
            };
        }
        VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bci.size = size;
        bci.usage = usage;
        VmaAllocationCreateInfo ainfo{};
        ainfo.usage = VMA_MEMORY_USAGE_AUTO;
        ainfo.flags = memoryUsage;
        vmaCreateBuffer(c->vulkan.allocator, &bci, &ainfo,&buffer,&allocation,&allocationInfo);
        return {};
    }

    Result<void> Buffer::copyToBuffer(char *data,uint32_t offset, uint32_t size) {
        auto c = context();
        if (!c) {
            if (ErrorHandeling::handleError(Error{ErrorCode::NotInitialized,"Renderer config not created. create with setRenderConfig."})) {
                return std::unexpected(Error{ErrorCode::NotInitialized,"Renderer config not created. create with setRenderConfig."});
            };
        }
        vmaCopyMemoryToAllocation(c->vulkan.allocator,data,allocation,offset,size);
        return {};
    }
}
