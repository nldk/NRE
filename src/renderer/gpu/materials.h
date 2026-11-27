#pragma once
#include <source_location>
#include "renderer.h"
#include "buffer.h"
#include "texture.h"
#include "UBOAndSSBO.h"

namespace NRE {
    // Set 1 of every pipeline: 0 base colour, 1 metallic-roughness, 2 normal (combined image samplers), 3 the
    // factors (GpuMaterial UBO). See shaders/include/nre/lighting.glsl.
    struct Material {
        // Empty: the defaults, white for base colour and metallic-roughness, a flat normal for normalTexture.
        Handel<Texture> baseTexture;       // sRGB
        Handel<Texture> metallicTexture;   // glTF layout (g = roughness, b = metallic), loaded with srgb = false
        Handel<Texture> normalTexture;     // tangent space, loaded with srgb = false
        // Factors, multiplied with the textures (glTF's defaults).
        glm::vec4 baseColor{1.0f};
        float metallic = 1.0f;
        float roughness = 1.0f;
        float alphaCutoff = 0.5f;
        float normalScale = 1.0f;
        bool alphaMask = false;     // discard where alpha < alphaCutoff
        bool doubleSided = false;   // drawn without culling; back faces get the flipped normal
        Buffer ubo;                 // the factors as a GpuMaterial
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

        // Creates the UBO and a set with bindings 0-3 written. Every call allocates a new set from the pool,
        // so call it once per material, after setting the fields: the factors are written now.
        // location: the API call to name in error messages (createMaterial() by default).
        Result<void> createDescriptorSet(std::source_location location = std::source_location::current());
        // Points binding 0, 1 or 2 at the matching texture field (its default when empty). Only while the GPU
        // isn't using the set.
        Result<void> writeTexture(uint32_t binding, std::source_location location = std::source_location::current());
        [[nodiscard]] GpuMaterial gpu() const;
    };
    // createMaterial is part of the public API, declared in renderer.h.
}
