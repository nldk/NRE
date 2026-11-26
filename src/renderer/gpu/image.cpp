#include "image.h"

#include <vulkan/vk_enum_string_helper.h>

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

    Result<void> Image::createImage(uint32_t width, uint32_t height, VkFormat format,VkImageUsageFlags usage, uint32_t mipLevels,
                                    bool cubemap) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        (void)destroyImage();   // creating it again replaces the old one

        this->format = format;
        this->mipLevels = mipLevels;
        this->extent = {width,height};
        this->cubemap = cubemap;
        const uint32_t layers = getLayers();

        VkImageCreateInfo vkImageCreateInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        vkImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        vkImageCreateInfo.format = format;
        vkImageCreateInfo.mipLevels = mipLevels;
        vkImageCreateInfo.extent.width = width;
        vkImageCreateInfo.extent.height = height;
        vkImageCreateInfo.extent.depth = 1;
        vkImageCreateInfo.arrayLayers = layers;
        if (cubemap) vkImageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        vkImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        vkImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        vkImageCreateInfo.usage = usage;



        VmaAllocationCreateInfo allocationCreateInfo = {};
        allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocationCreateInfo.flags = usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT : 0;

        if (VkResult r = vmaCreateImage(c->vulkan.allocator,&vkImageCreateInfo,&allocationCreateInfo,&image,&allocation,nullptr); r != VK_SUCCESS) {
            image = VK_NULL_HANDLE;
            allocation = nullptr;
            auto error = Error{ErrorCode::ImageCreationFailed, "Failed to create a " + std::to_string(width) + "x" + std::to_string(height)
                               + " " + string_VkFormat(format) + " image: " + std::string(ErrorHandeling::vkResultName(r))};
            if (ErrorHandeling::handleError(error)) {
                return std::unexpected(error);
            }
            return {};
        }

        VkImageViewCreateInfo imageViewCreateInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        imageViewCreateInfo.image = image;
        imageViewCreateInfo.viewType = cubemap ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = format;
        imageViewCreateInfo.subresourceRange = {aspectFor(format),0,mipLevels,0,layers};

        if (VkResult r = vkCreateImageView(c->vulkan.device,&imageViewCreateInfo,nullptr,&view); r != VK_SUCCESS) {
            // Don't leave a half-made image behind.
            view = VK_NULL_HANDLE;
            (void)destroyImage();
            auto error = Error{ErrorCode::ImageViewCreationFailed, "Failed to create the view of a " + std::to_string(width) + "x"
                               + std::to_string(height) + " " + string_VkFormat(format) + " image: " + std::string(ErrorHandeling::vkResultName(r))};
            if (ErrorHandeling::handleError(error)) {
                return std::unexpected(error);
            }
            return {};
        }

        if (usage & VK_IMAGE_USAGE_STORAGE_BIT) {
            imageViewCreateInfo.viewType = cubemap ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
            for (uint32_t level = 0; level < mipLevels; ++level) {
                imageViewCreateInfo.subresourceRange = {aspectFor(format), level, 1, 0, layers};
                VkImageView mipView = VK_NULL_HANDLE;
                if (VkResult r = vkCreateImageView(c->vulkan.device, &imageViewCreateInfo, nullptr, &mipView); r != VK_SUCCESS) {
                    (void)destroyImage();
                    auto error = Error{ErrorCode::ImageViewCreationFailed, "Failed to create the mip " + std::to_string(level)
                                       + " view of a " + std::to_string(width) + "x" + std::to_string(height) + " "
                                       + string_VkFormat(format) + " image: " + std::string(ErrorHandeling::vkResultName(r))};
                    if (ErrorHandeling::handleError(error)) {
                        return std::unexpected(error);
                    }
                    return {};
                }
                mipViews.push_back(mipView);
            }
        }

        return {};
    }

    Result<void> Image::destroyImage() {
        // Nothing to free: return before needing the context, which may already be gone (Images inside the Context
        // itself are destructed while it's being torn down).
        if (image == VK_NULL_HANDLE && view == VK_NULL_HANDLE && mipViews.empty()) return {};
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);

        for (VkImageView mipView : mipViews) vkDestroyImageView(c->vulkan.device, mipView, nullptr);
        mipViews.clear();
        if (view)  vkDestroyImageView(c->vulkan.device, view, nullptr);
        if (image) vmaDestroyImage(c->vulkan.allocator, image, allocation);
        image = VK_NULL_HANDLE; allocation = nullptr; view = VK_NULL_HANDLE;

        return {};
    }

    Image::~Image() {
        (void)destroyImage();
    }
}
