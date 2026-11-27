#pragma once
#include <vulkan/vulkan_core.h>

#include "gpu/image.h"
#include "pipeline/computePipeline.h"
#include "renderer.h"

namespace NRE {
    // Image-based light: the sky's light on objects, as nreAmbient reads it (set 0 bindings 4, 5, 6).
    //   irradiance: 32^2 cubemap, the diffuse light for each normal (cosine-weighted average radiance)
    //   specular:   128^2 cubemap with 5 mips, the sky prefiltered by GGX lobes of roughness 0, 0.25 .. 1
    //   BRDF LUT:   256^2, the split-sum environment BRDF (F0 scale and bias), shared by all (IblFilter)
    //
    // One Ibl per sky it is filtered from, not per scene (the sky itself is shared):
    //   - the procedural sky: one per frame slot, owned by the Sky next to that slot's sky cubemap and refiltered in
    //     the same command buffer right after the cubemap is regenerated (Sky::update). Slot f's frames are the only
    //     ones that read or write slot f's maps, and its previous frame finished before recording (its fence), so no
    //     frame in flight ever reads maps being rewritten.
    //   - a skybox: one per cubemap, owned by the Cubemap, filtered once (blocking) the first time it's made a
    //     skybox (setSkybox). Its contents never change afterwards, and it's destroyed with the cubemap
    //     (destroyCubemap waits for the GPU first).
    class Ibl {
        Image irradiance, specular;
        // The filter passes' sets, from this Ibl's own small pool, so destroying it gives them back.
        VkDescriptorPool pool = VK_NULL_HANDLE;
        VkDescriptorSet sourceSet = VK_NULL_HANDLE;   // iblSource.comp (filterLdr)
        VkDescriptorSet irradianceSet = VK_NULL_HANDLE;
        VkDescriptorSet prefilterSets[5]{};
        VkImageView setsSource = VK_NULL_HANDLE;      // the source the irradiance and prefilter sets read
    public:
        static constexpr uint32_t irradianceSize = 32;
        static constexpr uint32_t specularSize = 128, specularMipCount = 5;
        // The size of the linear HDR cubemap the filters read (the procedural sky's, and the copy filterLdr makes).
        static constexpr uint32_t sourceSize = 256;

        Ibl() = default;
        Ibl(const Ibl&) = delete;
        Ibl& operator=(const Ibl&) = delete;
        ~Ibl() { destroy(); }

        // The two cubemaps (every mip in SHADER_READ_ONLY_OPTIMAL, contents undefined until filtered) and the filter
        // passes' sets. Needs the IblFilter (VulkanContext::iblFilter) created first.
        Result<void> create();
        // Records the filtering of source (a linear HDR cubemap of sourceSize^2 with a full mip chain, in
        // SHADER_READ_ONLY_OPTIMAL and visible to compute shaders) into this Ibl's maps, then a barrier so the
        // fragment shaders after it read them. Outside any render pass. Only while no submitted frame still reads
        // this Ibl's maps (it rewrites them, and its sets when source changed).
        void filter(VkCommandBuffer commandBuffer, VkImageView source);
        // Filters an LDR skybox (display values, 8-bit, one mip, faces ldrSize^2): expanded with the inverse tone
        // curve into a temporary HDR source first. Submits and waits (load-time work, like an upload).
        Result<void> filterLdr(VkImageView ldrSource, uint32_t ldrSize);

        VkImageView irradianceView() const { return irradiance.getView(); }
        VkImageView specularView() const { return specular.getView(); }
        VkImageView brdfLutView() const;
        uint32_t specularMips() const { return specularMipCount; }
        // False until create() succeeded (it may fail under RenderErrorPolicy::Ignore).
        bool ready() const { return prefilterSets[specularMipCount - 1] != VK_NULL_HANDLE; }
        void destroy();
    };

    // What every Ibl shares: the filter passes, the BRDF LUT (computed once, in create()) and a sampler for all of
    // them (linear, every mip, clamped: the LUT's edges mustn't wrap). Lives in the VulkanContext; shutdown() calls
    // destroy().
    class IblFilter {
        ComputePipeline sourcePass, irradiancePass, prefilterPass, lutPass;
        Image brdfLut;
        VkSampler linearClamp = VK_NULL_HANDLE;
        friend class Ibl;
    public:
        static constexpr uint32_t brdfLutSize = 256;
        Result<void> create();
        VkImageView brdfLutView() const { return brdfLut.getView(); }
        VkSampler sampler() const { return linearClamp; }
        // False when create() failed under RenderErrorPolicy::Ignore: scenes then light with the sky colour's hemisphere.
        bool ready() const;
        void destroy();
    };
}
