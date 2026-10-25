#include "gpu/texture.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <filesystem>
#include <string>

#include "buffer.h"
#include "renderer.h"
#include "upload.h"
#include "core/context.h"
#include "core/errorHandling.h"

namespace NRE {
    Result<void> Texture::loadTexture(std::string path) {
        if (!std::filesystem::exists(path)) {
            auto error = Error{ErrorCode::FileNotFound, "Texture file not found: '" + std::filesystem::absolute(path).string() + "'"};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            return {};
        }
        int w, h, channels;
        stbi_uc* pixels = stbi_load(path.c_str(), &w, &h, &channels, STBI_rgb_alpha);
        if (!pixels) {
            // stb's reasons are terse ("bad png sig", "unknown image type"), so say which file it was.
            auto error = Error(ErrorCode::ErrorCreatingTexture, "Failed to load texture '" + path + "': "
                               + stbi_failure_reason() + " (supported: PNG, JPEG, BMP, TGA, PSD, GIF, HDR, PIC, PNM)");
                if (ErrorHandeling::handleError(error)) {
                    return std::unexpected(error);
                }
                return {};
        }
        auto upload = uploadPixels(pixels, uint32_t(w), uint32_t(h));
        stbi_image_free(pixels);
        return upload;
    }

    Result<void> Texture::loadTextureFromMemory(const unsigned char* data, size_t size) {
        int w, h, channels;
        stbi_uc* pixels = stbi_load_from_memory(data, int(size), &w, &h, &channels, STBI_rgb_alpha);
        if (!pixels) {
            auto error = Error(ErrorCode::ErrorCreatingTexture, "Failed to decode an embedded image (" + std::to_string(size)
                               + " bytes): " + stbi_failure_reason());
                if (ErrorHandeling::handleError(error)) {
                    return std::unexpected(error);
                }
                return {};
        }
        auto upload = uploadPixels(pixels, uint32_t(w), uint32_t(h));
        stbi_image_free(pixels);
        return upload;
    }

    Result<void> Texture::uploadPixels(const void* rgba, uint32_t w, uint32_t h, VkFormat format) {
        VkDeviceSize size = VkDeviceSize(w) * h * 4;
        Buffer stagingBuffer;
        if (auto r = stagingBuffer.createBuffer(size,VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT); !r) {
            return r;
        }
        if (auto r = stagingBuffer.copyToBuffer(rgba,0,size); !r) {
            return r;
        }

        if (auto r = image.createImage(w, h, format,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 1); !r) {
            return r;
        }

        return immediateSubmit([&](VkCommandBuffer cmd) {
            transition(cmd,image.getImage(),VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            VkBufferImageCopy region = {};
            region.bufferOffset = 0;
            region.bufferImageHeight = 0;
            region.bufferRowLength = 0;
            region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
            region.imageExtent = {w,h,1};
            region.imageOffset = {0,0,0};

            vkCmdCopyBufferToImage(cmd,stagingBuffer.buffer,image.getImage(),VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            transition(cmd, image.getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        });
    }

    Result<void> Texture::createSampler() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);

        float maxAniso = c->vulkan.physicalDevice.properties.limits.maxSamplerAnisotropy;

        VkSamplerCreateInfo samplerInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = std::min(16.0f, maxAniso);

        if (VkResult r = vkCreateSampler(c->vulkan.device,&samplerInfo,nullptr,&c->vulkan.sampler); r != VK_SUCCESS) {
            c->vulkan.sampler = VK_NULL_HANDLE;
            auto error = Error{ErrorCode::ErrorCreatingTexture, "init(): failed to create the texture sampler: "
                               + std::string(ErrorHandeling::vkResultName(r))};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
        }
        return {};
    }
    Result<VkDescriptorSet> Texture::createDescriptorSet() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        auto set = c->vulkan.descriptorPool.allocateDescriptorSet(c->vulkan.descriptorLayouts[1]);   // the material layout
        if (!set) return set;
        auto err = writeImageDescriptor(*set,0,image.getView(),c->vulkan.sampler);
        if (!err) return std::unexpected(err.error());
        return {set.value()};
    }
    Result<void> Texture::createTexture(std::string path) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        auto err = loadTexture(path);
        if (!err) return err;
        return {};
    }

    Result<void> Texture::destroySampler() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        vkDestroySampler(c->vulkan.device,c->vulkan.sampler,nullptr);
        return {};
    }
}
