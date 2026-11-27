#include "lighting/ibl.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <iterator>
#include <string>

#include "core/context.h"
#include "core/errorHandling.h"
#include "gpu/descriptorSet.h"
#include "gpu/upload.h"

namespace NRE {
    namespace {
        constexpr VkFormat iblFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        constexpr uint32_t groupSize = 8;   // local_size_x/y of every IBL pass
        constexpr uint32_t prefilterSamples = 64;
        // irradiance.comp integrates over every texel of the source's 16^2 mip (1536 of them).
        constexpr uint32_t irradianceSourceSize = 16;

        // Match the push constant blocks of the shaders.
        struct SourcePush { int32_t subSamples; };
        struct IrradiancePush { float lod; int32_t size; };
        struct PrefilterPush { float roughness; int32_t sampleCount; float sourceSize, sourceMips; };

        // Every pass: the source cubemap (or skybox) sampled, one storage image written.
        constexpr VkDescriptorSetLayoutBinding passBindings[] = {
            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };
        // An Ibl's sets: the source pass, the irradiance pass and one per specular mip.
        constexpr uint32_t setsPerIbl = 2 + Ibl::specularMipCount;

        uint32_t groups(uint32_t size) { return (size + groupSize - 1) / groupSize; }
        uint32_t fullMipCount(uint32_t size) { return uint32_t(std::bit_width(size)); }

        // Every level of a freshly made cubemap to SHADER_READ_ONLY_OPTIMAL, so descriptors may point at it at once.
        void toShaderRead(VkCommandBuffer cmd, const Image& image) {
            imageBarrier(cmd, image.getImage(), 0, image.getMipLevels(), 6, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
        }
    }

    // ---- IblFilter ---------------------------------------------------------------------------------------------

    Result<void> IblFilter::create() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        destroy();

        VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sci.magFilter = sci.minFilter = VK_FILTER_LINEAR;
        sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.maxLod = VK_LOD_CLAMP_NONE;
        if (VkResult r = vkCreateSampler(c->vulkan.device, &sci, nullptr, &linearClamp); r != VK_SUCCESS) {
            linearClamp = VK_NULL_HANDLE;
            auto error = Error{ErrorCode::ErrorCreatingTexture, "init(): failed to create the IBL sampler: "
                               + std::string(ErrorHandeling::vkResultName(r))};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            return {};   // no IBL: scenes light with the sky colour's hemisphere
        }

        static constexpr uint32_t iblSourceComp[] = {
#include "embeddedShaders/iblSource.comp.inc"
        };
        static constexpr uint32_t irradianceComp[] = {
#include "embeddedShaders/irradiance.comp.inc"
        };
        static constexpr uint32_t prefilterComp[] = {
#include "embeddedShaders/prefilter.comp.inc"
        };
        static constexpr uint32_t brdfLutComp[] = {
#include "embeddedShaders/brdfLut.comp.inc"
        };
        const VkDescriptorSetLayoutBinding lutBindings[] = {
            {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };
        if (auto r = sourcePass.create(iblSourceComp, passBindings, sizeof(SourcePush)); !r) return r;
        if (auto r = irradiancePass.create(irradianceComp, passBindings, sizeof(IrradiancePush)); !r) return r;
        if (auto r = prefilterPass.create(prefilterComp, passBindings, sizeof(PrefilterPush)); !r) return r;
        if (auto r = lutPass.create(brdfLutComp, lutBindings, 0); !r) return r;
        if (!sourcePass.ready() || !irradiancePass.ready() || !prefilterPass.ready() || !lutPass.ready()) return {};   // Ignore: reported

        if (auto r = brdfLut.createImage(brdfLutSize, brdfLutSize, iblFormat, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 1); !r) return r;
        if (brdfLut.getImage() == VK_NULL_HANDLE) return {};
        // The LUT depends on nothing but the BRDF: computed once, here. Its set is only needed for that.
        auto set = lutPass.allocateSet();
        if (!set) return std::unexpected(set.error());
        if (auto r = writeStorageImageDescriptor(*set, 0, brdfLut.getMipView(0)); !r) return r;
        return immediateSubmit([&](VkCommandBuffer cmd) {
            imageBarrier(cmd, brdfLut.getImage(), 0, 1, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                         VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
            lutPass.dispatch(cmd, *set, nullptr, groups(brdfLutSize), groups(brdfLutSize), 1);
            imageBarrier(cmd, brdfLut.getImage(), 0, 1, 1, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                         VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
        });
    }

    bool IblFilter::ready() const {
        return linearClamp != VK_NULL_HANDLE && sourcePass.ready() && irradiancePass.ready() && prefilterPass.ready()
               && lutPass.ready() && brdfLut.getImage() != VK_NULL_HANDLE;
    }

    void IblFilter::destroy() {
        auto c = context();
        if (!c) return;
        sourcePass.destroy();
        irradiancePass.destroy();
        prefilterPass.destroy();
        lutPass.destroy();   // its one set goes with the descriptor pool
        (void)brdfLut.destroyImage();
        if (linearClamp) vkDestroySampler(c->vulkan.device, linearClamp, nullptr);
        linearClamp = VK_NULL_HANDLE;
    }

    // ---- Ibl ---------------------------------------------------------------------------------------------------

    Result<void> Ibl::create() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        destroy();
        const IblFilter& f = c->vulkan.iblFilter;
        if (!f.ready()) return {};   // its failure was reported; nothing can be filtered

        constexpr VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        if (auto r = irradiance.createImage(irradianceSize, irradianceSize, iblFormat, usage, 1, true); !r) return r;
        if (auto r = specular.createImage(specularSize, specularSize, iblFormat, usage, specularMipCount, true); !r) return r;
        if (irradiance.getImage() == VK_NULL_HANDLE || specular.getImage() == VK_NULL_HANDLE) return {};   // Ignore: reported

        const VkDescriptorPoolSize sizes[] = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, setsPerIbl},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, setsPerIbl},
        };
        VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pci.maxSets = setsPerIbl;
        pci.poolSizeCount = uint32_t(std::size(sizes));
        pci.pPoolSizes = sizes;
        if (VkResult r = vkCreateDescriptorPool(c->vulkan.device, &pci, nullptr, &pool); r != VK_SUCCESS) {
            pool = VK_NULL_HANDLE;
            auto error = Error{ErrorCode::DescriptorCreationFailed, "Failed to create the IBL's descriptor pool: "
                               + std::string(ErrorHandeling::vkResultName(r))};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            return {};
        }
        auto allocate = [&](const ComputePipeline& pass, VkDescriptorSet& out) -> Result<void> {
            auto set = pass.allocateSet(pool);
            if (!set) return std::unexpected(set.error());
            out = *set;
            return {};
        };
        if (auto r = allocate(f.sourcePass, sourceSet); !r) return r;
        if (auto r = allocate(f.irradiancePass, irradianceSet); !r) return r;
        if (auto r = writeStorageImageDescriptor(irradianceSet, 1, irradiance.getMipView(0)); !r) return r;
        for (uint32_t mip = 0; mip < specularMipCount; ++mip) {
            if (auto r = allocate(f.prefilterPass, prefilterSets[mip]); !r) return r;
            if (auto r = writeStorageImageDescriptor(prefilterSets[mip], 1, specular.getMipView(mip)); !r) return r;
        }
        return immediateSubmit([&](VkCommandBuffer cmd) {
            toShaderRead(cmd, irradiance);
            toShaderRead(cmd, specular);
        });
    }

    void Ibl::filter(VkCommandBuffer cmd, VkImageView source) {
        if (!ready()) return;
        auto c = context();
        const IblFilter& f = c->vulkan.iblFilter;
        if (source != setsSource) {
            (void)writeImageDescriptor(irradianceSet, 0, source, f.linearClamp);
            for (VkDescriptorSet set : prefilterSets) (void)writeImageDescriptor(set, 0, source, f.linearClamp);
            setsSource = source;
        }

        // Both are rewritten whole. Their last readers were frames that have finished (see the class comment).
        imageBarrier(cmd, irradiance.getImage(), 0, 1, 6, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                     VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
        imageBarrier(cmd, specular.getImage(), 0, specularMipCount, 6, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                     VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);

        const IrradiancePush irradiancePush{std::log2(float(sourceSize) / float(irradianceSourceSize)), int32_t(irradianceSourceSize)};
        f.irradiancePass.dispatch(cmd, irradianceSet, &irradiancePush, groups(irradianceSize), groups(irradianceSize), 6);
        for (uint32_t mip = 0; mip < specularMipCount; ++mip) {
            const uint32_t size = specularSize >> mip;
            const PrefilterPush push{float(mip) / float(specularMipCount - 1), int32_t(prefilterSamples), float(sourceSize),
                                     float(fullMipCount(sourceSize))};
            f.prefilterPass.dispatch(cmd, prefilterSets[mip], &push, groups(size), groups(size), 6);
        }

        // Read by the main pass's fragment shaders (nreAmbient).
        imageBarrier(cmd, irradiance.getImage(), 0, 1, 6, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                     VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
        imageBarrier(cmd, specular.getImage(), 0, specularMipCount, 6, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                     VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    }

    Result<void> Ibl::filterLdr(VkImageView ldrSource, uint32_t ldrSize) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        if (!ready()) return {};
        const IblFilter& f = c->vulkan.iblFilter;

        // The HDR copy the filters read: sourceSize^2 with a full mip chain, only until the submit below is done.
        Image hdr;
        const uint32_t mips = fullMipCount(sourceSize);
        if (auto r = hdr.createImage(sourceSize, sourceSize, iblFormat, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                                     | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, mips, true); !r) return r;
        if (hdr.getImage() == VK_NULL_HANDLE) return {};   // Ignore: reported
        if (auto r = writeImageDescriptor(sourceSet, 0, ldrSource, f.linearClamp); !r) return r;
        if (auto r = writeStorageImageDescriptor(sourceSet, 1, hdr.getMipView(0)); !r) return r;
        // Each HDR texel averages the skybox texels under it (up to 8 x 8; larger skyboxes are sampled more sparsely).
        const SourcePush push{int32_t(std::clamp((ldrSize + sourceSize - 1) / sourceSize, 1u, 8u))};
        auto submitted = immediateSubmit([&](VkCommandBuffer cmd) {
            imageBarrier(cmd, hdr.getImage(), 0, 1, 6, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                         VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
            f.sourcePass.dispatch(cmd, sourceSet, &push, groups(sourceSize), groups(sourceSize), 6);
            recordCubemapMips(cmd, hdr.getImage(), sourceSize, mips);
            filter(cmd, hdr.getView());
        });
        // hdr is destroyed on return, after the GPU finished with it (immediateSubmit waits). The sets that read it
        // are rewritten before any further use: a later view could get the same handle value.
        setsSource = VK_NULL_HANDLE;
        return submitted;
    }

    VkImageView Ibl::brdfLutView() const {
        auto c = context();
        return c ? c->vulkan.iblFilter.brdfLutView() : VK_NULL_HANDLE;
    }

    void Ibl::destroy() {
        auto c = context();
        if (!c) return;
        if (pool) vkDestroyDescriptorPool(c->vulkan.device, pool, nullptr);   // frees the sets
        pool = VK_NULL_HANDLE;
        sourceSet = irradianceSet = VK_NULL_HANDLE;
        for (VkDescriptorSet& set : prefilterSets) set = VK_NULL_HANDLE;
        setsSource = VK_NULL_HANDLE;
        (void)irradiance.destroyImage();
        (void)specular.destroyImage();
    }
}
