#include "gpu/cubemap.h"

#include <stb_image.h>

#include <cstring>
#include <filesystem>
#include <vector>

#include "buffer.h"
#include "upload.h"
#include "core/context.h"
#include "core/errorHandling.h"

namespace NRE {
    namespace {
        constexpr const char* faceNames[6] = {"right (+X)", "left (-X)", "top (+Y)", "bottom (-Y)", "front (+Z)", "back (-Z)"};
    }

    Result<void> Cubemap::load(const std::array<std::string, 6>& paths) {
        // Decode all six first: their sizes have to match before anything goes to the GPU.
        std::vector<unsigned char> pixels;
        int size = 0;
        for (int face = 0; face < 6; ++face) {
            const std::string& path = paths[face];
            if (!std::filesystem::exists(path)) {
                auto error = Error{ErrorCode::FileNotFound, "createCubemap(): " + std::string(faceNames[face])
                                   + " face not found: '" + std::filesystem::absolute(path).string() + "'"};
                if (ErrorHandeling::handleError(error)) return std::unexpected(error);
                return {};
            }
            int w, h, channels;
            stbi_uc* data = stbi_load(path.c_str(), &w, &h, &channels, STBI_rgb_alpha);
            if (!data) {
                auto error = Error{ErrorCode::ErrorCreatingTexture, "createCubemap(): failed to load the " + std::string(faceNames[face])
                                   + " face '" + path + "': " + stbi_failure_reason()};
                if (ErrorHandeling::handleError(error)) return std::unexpected(error);
                return {};
            }
            if (face == 0) {
                size = w;
                pixels.resize(size_t(w) * h * 4 * 6);
            }
            if (w != h || w != size) {
                stbi_image_free(data);
                auto error = Error{ErrorCode::ErrorCreatingTexture, "createCubemap(): every face must be square and the same size. '"
                                   + paths[0] + "' is " + std::to_string(size) + "x" + std::to_string(size) + ", but the "
                                   + faceNames[face] + " face '" + path + "' is " + std::to_string(w) + "x" + std::to_string(h) + "."};
                if (ErrorHandeling::handleError(error)) return std::unexpected(error);
                return {};
            }
            const size_t faceBytes = size_t(size) * size * 4;
            std::memcpy(pixels.data() + face * faceBytes, data, faceBytes);
            stbi_image_free(data);
        }
        return uploadFaces(pixels.data(), uint32_t(size));
    }

    Result<void> Cubemap::createStorage(uint32_t size, VkFormat format, uint32_t mipLevels) {
        if (mipLevels == 0) {
            mipLevels = 1;
            while ((size >> mipLevels) > 0) ++mipLevels;
        }
        constexpr VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                                          | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (auto r = image.createImage(size, size, format, usage, mipLevels, true); !r) return r;
        if (image.getImage() == VK_NULL_HANDLE) return {};   // RenderErrorPolicy::Ignore: createImage already reported it
        return immediateSubmit([&](VkCommandBuffer cmd) {
            VkImageMemoryBarrier2 b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
            b.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
            b.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            b.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
            b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image = image.getImage();
            b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 6};
            VkDependencyInfo d{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            d.imageMemoryBarrierCount = 1;
            d.pImageMemoryBarriers = &b;
            vkCmdPipelineBarrier2(cmd, &d);
        });
    }

    Result<void> Cubemap::uploadFaces(const void* rgba, uint32_t size, VkFormat format) {
        const VkDeviceSize faceBytes = VkDeviceSize(size) * size * 4;
        Buffer staging;
        if (auto r = staging.createBuffer(faceBytes * 6, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT); !r) {
            return r;
        }
        if (auto r = staging.copyToBuffer(rgba, 0, faceBytes * 6); !r) return r;
        if (auto r = image.createImage(size, size, format, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 1, true); !r) {
            return r;
        }
        if (image.getImage() == VK_NULL_HANDLE) return {};   // RenderErrorPolicy::Ignore: createImage already reported it

        return immediateSubmit([&](VkCommandBuffer cmd) {
            transition(cmd, image.getImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, 6);
            VkBufferImageCopy regions[6]{};
            for (uint32_t face = 0; face < 6; ++face) {
                regions[face].bufferOffset = face * faceBytes;
                regions[face].imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, face, 1};
                regions[face].imageExtent = {size, size, 1};
            }
            vkCmdCopyBufferToImage(cmd, staging.buffer, image.getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, regions);
            transition(cmd, image.getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_IMAGE_ASPECT_COLOR_BIT, 6);
        });
    }

    Result<void> Cubemap::filterIbl() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        if (ibl || image.getImage() == VK_NULL_HANDLE || !c->vulkan.iblFilter.ready()) return {};
        auto filtered = std::make_unique<Ibl>();
        if (auto r = filtered->create(); !r) return r;
        if (!filtered->ready()) return {};   // RenderErrorPolicy::Ignore: reported; the scene lights with its sky colour
        if (auto r = filtered->filterLdr(image.getView(), getSize()); !r) return r;
        ibl = std::move(filtered);
        return {};
    }
}
