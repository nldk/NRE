#pragma once
#include <array>
#include <string>
#include <vulkan/vulkan_core.h>

#include "image.h"
#include "renderer.h"

namespace NRE {
    // Six square faces in one image with a cube view, sampled as a samplerCube. Used for the scene's skybox.
    class Cubemap {
        Image image;
    public:
        // Faces in Vulkan's (and OpenGL's) layer order: +X, -X, +Y, -Y, +Z, -Z. Loaded as color (sRGB).
        Result<void> load(const std::array<std::string, 6>& paths);
        // size x size RGBA8 pixels per face, faces back to back in the same order.
        Result<void> uploadFaces(const void* rgba, uint32_t size, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
        [[nodiscard]] VkImageView getView() const { return image.getView(); }
    };
}
