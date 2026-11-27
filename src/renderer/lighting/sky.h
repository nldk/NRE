#pragma once
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

#include "core/frames.h"
#include "gpu/cubemap.h"
#include "gpu/image.h"
#include "lighting/ibl.h"
#include "pipeline/computePipeline.h"
#include "renderer.h"

namespace NRE {
    // The scene's sky: the procedural atmosphere (Landscape) and the pass that draws the sky behind everything (any
    // sky source but the plain colour). Lives in the VulkanContext, shared by all scenes; shutdown() calls destroy().
    //
    // The procedural sky is computed with two compute passes: a transmittance LUT (256x64, once in create()) and the
    // sky cubemap (256^2 per face, RGBA16F, with a full mip chain) from it. The cubemap holds the finished linear
    // radiance, as drawn: the sunlit part (white sun) times the scattering brightness (sun intensity / 3,
    // FrameData::skyParams.z), plus the night floor, which doesn't depend on the intensity (so a sunless Landscape
    // scene shows night, not black). The sun's disk isn't in it: sky.frag draws it from FrameData::sunRadiance.
    //
    // Frames in flight: there's one cubemap per frame slot (currentFrame % max_frames_in_flight), so a frame only
    // ever writes the cubemap that its own slot's previous frame (whose fence was waited on) read: never one a frame
    // still on the GPU uses. The scene's set 0 binding 2 of slot f points at cubemapView(f). The sky is regenerated
    // when the brightness changed or the sun moved more than regenerateAngle from the sun of the last regeneration
    // (with brightness 0 the sun's direction doesn't matter); each slot then catches up once to that same sky, so all
    // slots show the same sky and consecutive frames never alternate between two. Each slot's IBL (its light on
    // objects) is refiltered from that slot's cubemap right after it's regenerated, so it always matches it.
    //
    // Frame flow (recordFrame): update() before the shadow and main passes, in the same command buffer, when the
    // bound scene shows the procedural sky; draw() in the main pass after the geometry.
    class Sky {
        Image transmittance;                          // the LUT: storage + sampled
        Cubemap cubemaps[max_frames_in_flight];       // one per frame slot
        Ibl ibls[max_frames_in_flight];               // each filtered from the slot's cubemap, right after it
        ComputePipeline lutPass, cubemapPass;
        VkDescriptorSet cubemapSets[max_frames_in_flight]{};
        VkSampler lutSampler = VK_NULL_HANDLE;        // linear, clamped to the edge
        Handel<Pipeline> drawPipeline;                // sky.vert + sky.frag, in the resource manager
        // What a cubemap was computed for: toward the sun, and the scattering brightness.
        struct Generated {
            glm::vec3 toSun{0.0f};
            float brightness = -1.0f;   // never a real brightness: nothing computed yet
            bool operator==(const Generated&) const = default;
        };
        Generated target;                             // the last regeneration's; every slot catches up to it
        Generated slots[max_frames_in_flight];        // what each slot's cubemap holds
    public:
        static constexpr uint32_t lutWidth = 256, lutHeight = 64;
        static constexpr uint32_t cubemapSize = 256;
        static constexpr float regenerateAngle = 0.25f * 3.14159265f / 180.0f;   // radians
        // The LUT (computed here, waiting for it), the cubemaps and IBLs (all mips in SHADER_READ_ONLY_OPTIMAL,
        // contents undefined until update()), the compute pipelines and the draw pipeline. Needs the descriptor pool,
        // the shared sampler and the IblFilter.
        Result<void> create();
        // Recomputes this frame slot's cubemap if it's behind the sun (see above): the compute pass, then the mips
        // (blits), then this slot's IBL from it, with barriers so the fragment shaders after it read both. Outside any
        // render pass. sunDirection: the way the sunlight travels; brightness: the scattering brightness, sun
        // intensity / 3 (0: night floor only). True when it recomputed them.
        bool update(VkCommandBuffer commandBuffer, glm::vec3 sunDirection, float brightness);
        // The sky behind what the main pass drew so far, for the scene's sky source this frame (writeFrameData):
        // cubemap or procedural; nothing for the plain colour (the clear colour is the sky then). Binds its pipeline
        // and the scene's set 0. Inside the main pass, Render_3D only.
        void draw(VkCommandBuffer commandBuffer, Scene& scene);
        // This frame slot's sky cubemap (what set 0 binding 2 shows in procedural mode).
        VkImageView cubemapView() const;
        // A frame slot's IBL (what set 0 bindings 4 and 5 show in procedural mode).
        const Ibl& ibl(uint32_t frameSlot) const { return ibls[frameSlot]; }
        VkImageView cubemapView(uint32_t frameSlot) const { return cubemaps[frameSlot].getView(); }
        VkImage cubemapImage(uint32_t frameSlot) const { return cubemaps[frameSlot].getImage(); }
        uint32_t cubemapMipLevels() const { return cubemaps[0].getMipLevels(); }
        // False when create() failed under RenderErrorPolicy::Ignore: scenes then fall back to the sky colour.
        bool ready() const;
        void destroy();
    };
}
