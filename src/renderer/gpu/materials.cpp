#include "materials.h"

#include "core/context.h"
#include "core/errorHandling.h"

namespace NRE {
    GpuMaterial Material::gpu() const {
        GpuMaterial g{};
        g.baseColorFactor = baseColor;
        g.metallic = metallic;
        g.roughness = roughness;
        g.alphaCutoff = alphaCutoff;
        g.normalScale = normalScale;
        g.flags = (alphaMask ? GpuMaterial::alphaMaskFlag : 0u) | (doubleSided ? GpuMaterial::doubleSidedFlag : 0u);
        return g;
    }

    Result<void> Material::writeTexture(uint32_t binding, std::source_location location) {
        auto c = context();
        NRE_REQUIRE_CONTEXT_AT(c, location);
        static constexpr const char* names[] = {"base texture", "metallic-roughness texture", "normal texture"};
        Handel<Texture> handle = binding == 0 ? baseTexture : binding == 1 ? metallicTexture : normalTexture;
        Handel<Texture> fallback = binding == 2 ? c->vulkan.flatNormalTexture : c->vulkan.whiteTexture;
        Texture* texture = c->manager.get(handle == Handel<Texture>{} ? fallback : handle);
        if (!texture) {
            auto error = ErrorHandeling::badHandle(names[binding], handle, "createTexture()", location);
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            texture = c->manager.get(fallback);   // keep the set valid
        }
        return writeImageDescriptor(descriptorSet, binding, texture->getView(), c->vulkan.sampler);
    }

    Result<void> Material::createDescriptorSet(std::source_location location) {
        auto c = context();
        NRE_REQUIRE_CONTEXT_AT(c, location);

        auto set = c->vulkan.descriptorPool.allocateDescriptorSet(c->vulkan.descriptorLayouts[1]);   // the material layout
        if (!set) return std::unexpected(set.error());
        descriptorSet = *set;
        for (uint32_t binding = 0; binding < 3; ++binding) {
            if (auto r = writeTexture(binding, location); !r) return r;
        }

        // Written once here: the factors don't change afterwards, so the GPU never reads it while it's rewritten.
        if (auto r = ubo.createBuffer(sizeof(GpuMaterial), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT); !r) return r;
        if (ubo.buffer == VK_NULL_HANDLE) {
            // Only reached under RenderErrorPolicy::Ignore, which already reported it: the set can't be completed.
            descriptorSet = VK_NULL_HANDLE;
            return {};
        }
        GpuMaterial factors = gpu();
        if (auto r = ubo.copyToBuffer(&factors, 0, sizeof(factors)); !r) return r;
        return writeBufferDescriptor(descriptorSet, 3, ubo.buffer, sizeof(GpuMaterial));
    }

    Result<Handel<Material>> createMaterial(Handel<Texture> baseTexture, Handel<Texture> metallicTexture,
                                            Handel<Texture> normalTexture) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);

        Material material;
        material.baseTexture = baseTexture;
        material.metallicTexture = metallicTexture;
        material.normalTexture = normalTexture;
        // glTF's metallic factor is 1, which with no metallic-roughness texture would make everything metal.
        // Without one the material is a plain (non-metal) surface.
        material.metallic = metallicTexture == Handel<Texture>{} ? 0.0f : 1.0f;
        if (auto r = material.createDescriptorSet(); !r) return std::unexpected(r.error());
        return c->manager.create(std::move(material));
    }
}
