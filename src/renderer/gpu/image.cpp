#include "image.h"

#include <string>

#include "core/context.h"

namespace NRE {
    VkImageAspectFlags aspectFor(VkFormat format) {
        switch (format) {
            case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_X8_D24_UNORM_PACK32:
            case VK_FORMAT_D32_SFLOAT:        return VK_IMAGE_ASPECT_DEPTH_BIT;
            default:                          return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }

    Result<void> Image::createImage(uint32_t width, uint32_t height, VkFormat format,VkImageUsageFlags usage, uint32_t mipLevels) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c,"Renderer not initialized. Initialize with init.");

        this->format = format;
        this->mipLevels = mipLevels;
        this->extent = {width,height};

        VkImageCreateInfo vkImageCreateInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        vkImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        vkImageCreateInfo.format = format;
        vkImageCreateInfo.mipLevels = mipLevels;
        vkImageCreateInfo.extent.width = width;
        vkImageCreateInfo.extent.height = height;
        vkImageCreateInfo.extent.depth = 1;
        vkImageCreateInfo.arrayLayers = 1;
        vkImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        vkImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        vkImageCreateInfo.usage = usage;



        VmaAllocationCreateInfo allocationCreateInfo = {};
        allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocationCreateInfo.flags = usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT : 0;

        if (vmaCreateImage(c->vulkan.allocator,&vkImageCreateInfo,&allocationCreateInfo,&image,&allocation,nullptr) != VK_SUCCESS) {
            image = VK_NULL_HANDLE;
            allocation = nullptr;
            auto error = Error{ErrorCode::ImageCreationFailed,"Failed to create " + std::to_string(width) + "x" + std::to_string(height) + " image."};
            if (ErrorHandeling::handleError(error)) {
                return std::unexpected(error);
            }
            return {};
        }

        VkImageViewCreateInfo imageViewCreateInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        imageViewCreateInfo.image = image;
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = format;
        imageViewCreateInfo.subresourceRange = {aspectFor(format),0,mipLevels,0,1};

        if (vkCreateImageView(c->vulkan.device,&imageViewCreateInfo,nullptr,&view) != VK_SUCCESS) {
            // Don't leave a half-made image behind.
            view = VK_NULL_HANDLE;
            (void)destroyImage();
            auto error = Error{ErrorCode::ImageViewCreationFailed,"Failed to create image view."};
            if (ErrorHandeling::handleError(error)) {
                return std::unexpected(error);
            }
            return {};
        }

        return {};
    }

    Result<void> Image::destroyImage() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c,"Renderer not initialized. Initialize with init.");

        if (view)  vkDestroyImageView(c->vulkan.device, view, nullptr);
        if (image) vmaDestroyImage(c->vulkan.allocator, image, allocation);
        image = VK_NULL_HANDLE; allocation = nullptr; view = VK_NULL_HANDLE;

        return {};
    }

    Image::~Image() {
        (void)destroyImage();
    }
}
