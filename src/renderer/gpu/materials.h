#pragma once
#include <source_location>
#include "renderer.h"
#include "texture.h"

namespace NRE {
    struct Material {
        Handel<Texture> baseTexture;
        Handel<Texture> metallicTexture;
        Handel<Texture> normalTexture;
        glm::vec4 baseColor{1.0f};
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

        // Gets a set with binding 0 pointing at baseTexture. Every call allocates a new set from the pool,
        // so call it once per material.
        // metallicTexture and normalTexture aren't written yet: the material layout only has the base color binding.
        // location: the API call to name in error messages (createMaterial() by default).
        Result<void> createDescriptorSet(std::source_location location = std::source_location::current());
    };
    // createMaterial is part of the public API, declared in renderer.h.
}
