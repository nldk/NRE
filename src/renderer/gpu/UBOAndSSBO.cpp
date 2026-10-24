#include "UBOAndSSBO.h"

#include <algorithm>
#include <string>

#include "descriptorSet.h"
#include "core/context.h"
#include "core/errorHandling.h"

namespace NRE {
    namespace {
        VkBufferUsageFlags usageFor(VkDescriptorType type) {
            return type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        }
    }

    Result<void> PerFrameBuffer::create(VkDescriptorType type, VkDeviceSize size) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
        #ifndef NDEBUG
        if (!(type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER )) {
            auto err = Error{ErrorCode::ImpossibleTypeForPerFrameBuffer,"The type given is not for creating a uniform buffer/storage buffer."};
            ErrorHandeling::handleError(err);
            return std::unexpected(err);
        }
        if (size <= 0) {
            auto err = Error{ErrorCode::BufferWithSizeNull,"Supplied size is less or equal than zero."};
            ErrorHandeling::handleError(err);
            return std::unexpected(err);
        }
        if (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER && size > c->vulkan.physicalDevice.properties.limits.maxUniformBufferRange) {
            auto err = Error{ErrorCode::SizeOfBufferToBig, "The supplied size for creating a UBO is bigger than the max size on the GPU."};
            ErrorHandeling::handleError(err);
            return std::unexpected(err);
        }
        #endif
        (void)destroy();
        this->type = type;
        for (int i = 0; i <max_frames_in_flight; i++) {
            auto err = buffers[i].createBuffer(size,usageFor(type),VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

            if (!err || buffers[i].buffer == VK_NULL_HANDLE) {
                (void)destroy();
                return err;
            }
            capacities[i] = size;
        }
        return {};
    }

    Result<void> PerFrameBuffer::attach(const VkDescriptorSet (&frameSets)[max_frames_in_flight], uint32_t binding) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
#ifndef NDEBUG
        if (!created()) {
            auto err = Error{ErrorCode::NotInitialized, "PerFrameBuffer::attach called before create."};
            ErrorHandeling::handleError(err);
            return std::unexpected(err);
        }
#endif
        this->binding = binding;
        for (int i = 0; i <max_frames_in_flight; i++) {
            sets[i] = frameSets[i];
            if (auto r = writeBufferDescriptor(sets[i], binding, buffers[i].buffer,capacities[i],0,type); !r) return r;
        }
        return {};
    }

    Result<void> PerFrameBuffer::write(uint32_t frame, const void *data, VkDeviceSize size) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
#ifndef NDEBUG
        if (frame >= max_frames_in_flight) {
            auto err = Error{ErrorCode::IndexOutOfRange, "Frame " + std::to_string(frame) + " doesn't exist; there are "
                             + std::to_string(max_frames_in_flight) + " frames in flight."};
            ErrorHandeling::handleError(err);
            return std::unexpected(err);
        }
        if (buffers[frame].buffer == VK_NULL_HANDLE) {
            auto err = Error{ErrorCode::NotInitialized, "PerFrameBuffer::write called before create."};
            ErrorHandeling::handleError(err);
            return std::unexpected(err);
        }
        if (!data && size > 0) {
            auto err = Error{ErrorCode::UploadFailed, "PerFrameBuffer::write got no data for " + std::to_string(size) + " bytes."};
            ErrorHandeling::handleError(err);
            return std::unexpected(err);
        }
        if (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER && size > capacities[frame]) {
            auto err = Error{ErrorCode::SizeOfBufferToBig, "Writing " + std::to_string(size) + " bytes into a "
                             + std::to_string(capacities[frame]) + " byte uniform buffer."};
            ErrorHandeling::handleError(err);
            return std::unexpected(err);
        }
#endif
        if (size == 0) return {};   // e.g. no lights this frame
        if (size > capacities[frame]) {
            if (auto r = grow(frame, size); !r) return r;
            if (size > capacities[frame]) return {};
        }
        return buffers[frame].copyToBuffer(data, 0, size);
    }

    Result<void> PerFrameBuffer::grow(uint32_t frame, VkDeviceSize needed) {
        VkDeviceSize newCapacity = std::max(needed, capacities[frame] * 2);
        Buffer bigger;
        auto created = bigger.createBuffer(newCapacity, usageFor(type), VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
        if (!created || bigger.buffer == VK_NULL_HANDLE) return created;
        buffers[frame] = std::move(bigger);
        capacities[frame] = newCapacity;
        if (sets[frame] == VK_NULL_HANDLE) return {};
        return writeBufferDescriptor(sets[frame], binding, buffers[frame].buffer, newCapacity, 0, type);
    }

    Result<void> PerFrameBuffer::destroy() {
        for (int i = 0; i < max_frames_in_flight; i++) {
            if (auto r = buffers[i].destroyBuffer(); !r) return r;
            capacities[i] = 0;
            sets[i] = VK_NULL_HANDLE;
        }
        return {};
    }
}
