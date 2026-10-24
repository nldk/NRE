#include "materials.h"

#include "core/context.h"
#include "core/errorHandling.h"

namespace NRE {
    Result<void> Material::createDescriptorSet() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");

        Texture* base = c->manager.get(baseTexture);
        if (!base) {
            auto error = Error{ErrorCode::HandelOutOfDateOrNonExistend, "Handel to material base texture out of date or non-existing"};
            if (ErrorHandeling::handleError(error)) {
                return std::unexpected(error);
            }
            return {};
        }

        // The material layout only has the base color binding, so the texture's set is the material's set.
        auto set = base->createDescriptorSet();
        if (!set) return std::unexpected(set.error());
        descriptorSet = *set;
        return {};
    }

    Result<Handel<Material>> createMaterial(Handel<Texture> baseTexture, Handel<Texture> metallicTexture,
                                            Handel<Texture> normalTexture) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");

        Material material;
        material.baseTexture = baseTexture == Handel<Texture>{} ? c->vulkan.defaultTexture : baseTexture;
        material.metallicTexture = metallicTexture;
        material.normalTexture = normalTexture;
        if (auto r = material.createDescriptorSet(); !r) return std::unexpected(r.error());
        return c->manager.create(std::move(material));
    }
}
