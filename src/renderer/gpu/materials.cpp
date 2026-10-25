#include "materials.h"

#include "core/context.h"
#include "core/errorHandling.h"

namespace NRE {
    Result<void> Material::createDescriptorSet(std::source_location location) {
        auto c = context();
        NRE_REQUIRE_CONTEXT_AT(c, location);

        Texture* base = c->manager.get(baseTexture);
        if (!base) {
            auto error = ErrorHandeling::badHandle("base texture", baseTexture, "createTexture()", location);
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
        NRE_REQUIRE_CONTEXT(c);

        Material material;
        material.baseTexture = baseTexture == Handel<Texture>{} ? c->vulkan.defaultTexture : baseTexture;
        material.metallicTexture = metallicTexture;
        material.normalTexture = normalTexture;
        if (auto r = material.createDescriptorSet(); !r) return std::unexpected(r.error());
        return c->manager.create(std::move(material));
    }
}
