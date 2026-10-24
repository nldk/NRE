#pragma once
#include <string>
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

#include "image.h"
#include "renderer.h"

namespace NRE {
    class Texture {
        uint32_t width,height = 0;
        Image image;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    public:
        Result<void> createTexture(std::string path);
        Result<void> loadTexture(std::string path);
        Result<void> loadTextureFromMemory(const unsigned char* data, size_t size);
        Result<void> uploadPixels(const void* rgba, uint32_t w, uint32_t h, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
        Result<VkDescriptorSet> createDescriptorSet();
        [[nodiscard]] VkDescriptorSet getDescriptorSet() const {return descriptorSet;};
        [[nodiscard]] VkImageView getView() const {return image.getView();};
        static Result<void> createSampler();
        static Result<void> destroySampler();
    };
}
