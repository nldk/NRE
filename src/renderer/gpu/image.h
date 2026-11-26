#pragma once
#include <utility>
#include <vulkan/vulkan_core.h>

#include "image.h"
#include "renderer.h"
#include "vk_mem_alloc.h"

namespace NRE {
    class Image {
        VkImage image = VK_NULL_HANDLE;
        VmaAllocation allocation = nullptr;
        VkImageView view = VK_NULL_HANDLE;
        VkExtent2D extent{};
        VkFormat format = VK_FORMAT_UNDEFINED;
        uint32_t mipLevels = 1;
        bool cubemap = false;
        public:
        // Empty image (VK_NULL_HANDLE); destroying it is a no-op.
        Image() = default;
        Image(uint32_t width,uint32_t height, VkFormat format,VkImageUsageFlags usage,uint32_t mipLevels = 1) {
            createImage(width,height,format,usage,mipLevels);
        };
        // cubemap: 6 layers (+X, -X, +Y, -Y, +Z, -Z) of width x height, with a cube view. Faces must be square.
        Result<void> createImage(uint32_t width, uint32_t height, VkFormat format,VkImageUsageFlags usage, uint32_t mipLevels,
                                 bool cubemap = false);
        Result<void> destroyImage();

        VkImage getImage() const { return image; }
        VkImageView getView() const { return view; }
        VkExtent2D getExtent() const { return extent; }
        VkFormat getFormat() const { return format; }
        uint32_t getMipLevels() const { return mipLevels; }
        uint32_t getLayers() const { return cubemap ? 6 : 1; }

        Image(const Image&) = delete;
        Image& operator=(const Image&) = delete;
        Image(Image&& o) noexcept
            : image(std::exchange(o.image, VK_NULL_HANDLE)),
              allocation(std::exchange(o.allocation, nullptr)),
              view(std::exchange(o.view, VK_NULL_HANDLE)),
              extent(o.extent),
              format(o.format),
              mipLevels(o.mipLevels),
              cubemap(o.cubemap) {}
        Image& operator=(Image&& o) noexcept {
            if (this != &o) {
                (void)destroyImage();
                image = std::exchange(o.image, VK_NULL_HANDLE);
                allocation = std::exchange(o.allocation, nullptr);
                view = std::exchange(o.view, VK_NULL_HANDLE);
                extent = o.extent;
                format = o.format;
                mipLevels = o.mipLevels;
                cubemap = o.cubemap;
            }
            return *this;
        }
        ~Image();
    };
}
