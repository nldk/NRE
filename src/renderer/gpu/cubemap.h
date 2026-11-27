#pragma once
#include <array>
#include <memory>
#include <string>
#include <vulkan/vulkan_core.h>

#include "image.h"
#include "lighting/ibl.h"
#include "renderer.h"

namespace NRE {
    // Six square faces in one image with a cube view, sampled as a samplerCube. Used for the scene's skybox, and
    // (createStorage) for cubemaps the renderer computes itself, like the procedural sky.
    class Cubemap {
        Image image;
        // As a skybox: its light on objects (filterIbl), made the first time it's needed and destroyed with it.
        std::unique_ptr<Ibl> ibl;
    public:
        // Faces in Vulkan's (and OpenGL's) layer order: +X, -X, +Y, -Y, +Z, -Z. Loaded as color (sRGB).
        Result<void> load(const std::array<std::string, 6>& paths);
        // size x size RGBA8 pixels per face, faces back to back in the same order.
        Result<void> uploadFaces(const void* rgba, uint32_t size, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
        // Empty faces for compute shaders to write (storage usage, a view per mip level: getMipView), also usable as
        // a blit source and destination (to build the mips). Every level starts in SHADER_READ_ONLY_OPTIMAL, so a
        // descriptor can point at it before anything was written. mipLevels 0: the full chain down to 1x1.
        Result<void> createStorage(uint32_t size, VkFormat format, uint32_t mipLevels = 1);
        [[nodiscard]] VkImageView getView() const { return image.getView(); }
        [[nodiscard]] VkImage getImage() const { return image.getImage(); }
        // createStorage only: one mip level, all 6 faces, as an image2DArray for storage writes.
        [[nodiscard]] VkImageView getMipView(uint32_t level) const { return image.getMipView(level); }
        [[nodiscard]] uint32_t getMipLevels() const { return image.getMipLevels(); }
        [[nodiscard]] uint32_t getSize() const { return image.getExtent().width; }
        // An LDR cubemap (load / uploadFaces) as a skybox: filters its IBL once (blocking), the first time; afterwards
        // does nothing. Its contents never change, so frames may read it as soon as this returns.
        Result<void> filterIbl();
        // The filtered IBL, or null if filterIbl() wasn't called or failed.
        [[nodiscard]] const Ibl* getIbl() const { return ibl.get(); }
    };
}
