#pragma once
#include <string>
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

#include "image.h"
#include "renderer.h"

namespace NRE {
    class Texture {
        Image image;
    public:
        // srgb: colour images (base colour) decode from sRGB on sampling. Data maps (normal, metallic-roughness)
        // hold plain numbers, so they load with srgb = false.
        Result<void> loadTexture(std::string path, bool srgb = true);
        Result<void> loadTextureFromMemory(const unsigned char* data, size_t size, bool srgb = true);
        Result<void> uploadPixels(const void* rgba, uint32_t w, uint32_t h, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
        [[nodiscard]] VkImageView getView() const {return image.getView();};
        static Result<void> createSampler();
        static Result<void> destroySampler();
    };
}
