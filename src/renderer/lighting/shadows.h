#pragma once
#include <span>
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

#include "core/frameRecorder.h"
#include "renderer.h"

namespace NRE {
    // The sun's cascaded shadow maps: one depth image (D32_SFLOAT) with a layer per cascade, sampled through set 0
    // binding 3 (nreShadowMap in nre/frame.glsl) with a comparison sampler. Lives in the VulkanContext, shared by
    // all scenes; shutdown() calls destroy(). Conventions: see LightingMath::cascadeViewProj.
    //
    // Frame flow: recordFrame() calls record() before the main pass, with the draws of the frame and the bound
    // scene's cascade count (Scene::shadowCascades, written with the cascade matrices by writeFrameData()).
    // Layers [0, cascadeCount) are cleared and drawn, then left in SHADER_READ_ONLY_OPTIMAL for the main pass;
    // the other layers stay in that layout from create().
    class ShadowMaps {
        VkImage image = VK_NULL_HANDLE;
        VmaAllocation allocation = nullptr;
        VkImageView array = VK_NULL_HANDLE;                 // all layers, for sampling
        VkImageView layerViews[4]{};                        // one per cascade, to render into
        VkSampler compareSampler = VK_NULL_HANDLE;
        Handel<Pipeline> depthOnly;    // vertex stage only
        Handel<Pipeline> alphaMasked;  // + shadowAlpha.frag, for alpha-masked materials
        uint32_t size = 0;
        uint32_t layers = 0;
    public:
        static constexpr uint32_t maxLayers = 4;
        // Depth bias of the shadow pipelines, on top of the normal offset in nreSunShadow (FrameData::shadowParams.y).
        static constexpr float depthBiasConstant = 1.25f;
        static constexpr float depthBiasSlope = 1.75f;
        // How far nreSunShadow moves the lookup off the surface, along its normal, in texels of the cascade
        // (FrameData::shadowParams.y).
        static constexpr float normalOffsetTexels = 1.5f;
        // The image (resolution² per layer), its views, the sampler and the two pipelines. The layers start in
        // SHADER_READ_ONLY_OPTIMAL, so the descriptor is valid before anything was drawn. maxCascades <= 4.
        Result<void> create(uint32_t resolution = 2048, uint32_t maxCascades = 4);
        // Draws every draw with castsShadows into layers [0, cascadeCount) (capped at the layer count), each with
        // nreFrame.cascadeViewProj[layer] from the scene's set 0. Outside any render pass. Does nothing for 0.
        void record(VkCommandBuffer commandBuffer, Scene& scene, std::span<const QueuedDraw> draws, uint32_t cascadeCount);
        void destroy();
        // False when create() failed under RenderErrorPolicy::Ignore: record() then draws nothing, so scenes must
        // use 0 cascades.
        bool ready() const;
        VkImageView arrayView() const { return array; }
        VkSampler sampler() const { return compareSampler; }
        uint32_t resolution() const { return size; }
        // How many cascades it can hold (0 if create() failed).
        uint32_t layerCount() const { return layers; }
    };
}
