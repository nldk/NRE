#include "lighting/shadows.h"

#include <algorithm>
#include <string>

#include "core/context.h"
#include "core/errorHandling.h"
#include "gpu/upload.h"

namespace NRE {
    namespace {
        // Matches the push constant block in shaders/shadow.vert: the model matrix (Model::drawDepthOnly), then this.
        constexpr uint32_t cascadePushOffset = sizeof(glm::mat4);

        // Position and UV only: the shadow shaders don't read the normal.
        VertexLayout shadowVertexLayout() {
            return {
                {{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX}},
                {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)},
                 {1, 0, VK_FORMAT_R32G32_SFLOAT,    offsetof(Vertex, uv)}},
            };
        }
    }

    Result<void> ShadowMaps::create(uint32_t resolution, uint32_t maxCascades) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        destroy();
        const uint32_t layerCount = std::clamp(maxCascades, 1u, maxLayers);

        VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.format = depthFormat;
        ici.extent = {resolution, resolution, 1};
        ici.mipLevels = 1;
        ici.arrayLayers = layerCount;
        ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        VmaAllocationCreateInfo aci{};
        aci.usage = VMA_MEMORY_USAGE_AUTO;
        aci.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        if (VkResult r = vmaCreateImage(c->vulkan.allocator, &ici, &aci, &image, &allocation, nullptr); r != VK_SUCCESS) {
            image = VK_NULL_HANDLE;
            allocation = nullptr;
            auto error = Error{ErrorCode::ImageCreationFailed, "init(): failed to create the sun's shadow maps ("
                               + std::to_string(layerCount) + " x " + std::to_string(resolution) + "x"
                               + std::to_string(resolution) + "): " + std::string(ErrorHandeling::vkResultName(r))};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            return {};   // no shadows: record() does nothing without the image
        }
        size = resolution;
        layers = layerCount;

        VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vci.image = image;
        vci.format = depthFormat;
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        vci.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, layers};
        VkResult r = vkCreateImageView(c->vulkan.device, &vci, nullptr, &array);
        for (uint32_t i = 0; i < layers && r == VK_SUCCESS; ++i) {
            vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            vci.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, i, 1};
            r = vkCreateImageView(c->vulkan.device, &vci, nullptr, &layerViews[i]);
        }
        if (r != VK_SUCCESS) {
            destroy();
            auto error = Error{ErrorCode::ImageViewCreationFailed, "init(): failed to create the shadow maps' image views: "
                               + std::string(ErrorHandeling::vkResultName(r))};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            return {};
        }

        // Comparison sampler: 1 where the looked-up depth <= the stored one (lit), bilinear over the 2x2 results.
        // Outside the map the border (depth 1, the far end) is lit.
        VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sci.magFilter = VK_FILTER_LINEAR;
        sci.minFilter = VK_FILTER_LINEAR;
        sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sci.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        sci.compareEnable = VK_TRUE;
        sci.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        sci.maxLod = 0.0f;
        if (VkResult sr = vkCreateSampler(c->vulkan.device, &sci, nullptr, &compareSampler); sr != VK_SUCCESS) {
            destroy();
            auto error = Error{ErrorCode::ErrorCreatingTexture, "init(): failed to create the shadow map sampler: "
                               + std::string(ErrorHandeling::vkResultName(sr))};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            return {};
        }

        // The main pass samples every layer's descriptor, so all of them start readable (their contents are never
        // read before record() draws them).
        auto uploaded = immediateSubmit([&](VkCommandBuffer cmd) {
            transition(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT, layers);
        });
        if (!uploaded) {
            destroy();
            return uploaded;
        }

        static constexpr uint32_t shadowVert[] = {
#include "embeddedShaders/shadow.vert.inc"
        };
        static constexpr uint32_t shadowAlphaFrag[] = {
#include "embeddedShaders/shadowAlpha.frag.inc"
        };
        BuiltinShader vertex(shadowVert, ShaderType::VERTEX_SHADER);
        BuiltinShader fragment(shadowAlphaFrag, ShaderType::FRAGMENT_SHADER);
        const VertexLayout layout = shadowVertexLayout();
        Pipeline opaque, masked;
        if (auto err = opaque.createDepthOnlyPipeline(vertex.handle, {}, layout, depthBiasConstant, depthBiasSlope); !err) return err;
        if (auto err = masked.createDepthOnlyPipeline(vertex.handle, fragment.handle, layout, depthBiasConstant, depthBiasSlope); !err) return err;
        // Under RenderErrorPolicy::Ignore a failed pipeline is empty; record() then draws no shadows.
        if (opaque.pipeline != VK_NULL_HANDLE && masked.pipeline != VK_NULL_HANDLE) {
            depthOnly = c->manager.create(std::move(opaque));
            alphaMasked = c->manager.create(std::move(masked));
        }
        return {};
    }

    void ShadowMaps::record(VkCommandBuffer cmd, Scene& scene, std::span<const QueuedDraw> draws, uint32_t cascadeCount) {
        auto c = context();
        cascadeCount = std::min(cascadeCount, layers);
        Pipeline* opaque = c->manager.get(depthOnly);
        Pipeline* masked = c->manager.get(alphaMasked);
        if (cascadeCount == 0 || !opaque || !masked) return;

        // Last frame's maps aren't needed: each layer drawn is cleared first.
        transition(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT, cascadeCount);
        const VkExtent2D extent{size, size};
        for (uint32_t cascade = 0; cascade < cascadeCount; ++cascade) {
            VkRenderingAttachmentInfo depth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
            depth.imageView = layerViews[cascade];
            depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            depth.clearValue.depthStencil = {1.0f, 0};   // standard depth: far is 1
            VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
            ri.renderArea = {{0, 0}, extent};
            ri.layerCount = 1;
            ri.pDepthAttachment = &depth;
            vkCmdBeginRendering(cmd, &ri);

            VkViewport viewport{0.0f, 0.0f, float(size), float(size), 0.0f, 1.0f};
            VkRect2D scissor{{0, 0}, extent};
            vkCmdSetViewport(cmd, 0, 1, &viewport);
            vkCmdSetScissor(cmd, 0, 1, &scissor);
            // Binds the depth-only pipeline and the scene's set 0 (the cascade matrices). Both shadow pipelines have
            // the same layout, so set 0, the push constants and the cull mode stay when drawDepthOnly switches.
            scene.bindFrameData(cmd, *opaque);
            VkPipeline bound = opaque->pipeline;
            // No culling: the light's projection has no Y flip, so winding is mirrored relative to the main pass,
            // and casting from both sides also covers open meshes. The pass starts with the cull mode unknown
            // (Model::unknownCullMode), so it's set here, once.
            vkCmdSetCullMode(cmd, VK_CULL_MODE_NONE);
            vkCmdPushConstants(cmd, opaque->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, cascadePushOffset, sizeof(cascade), &cascade);
            for (const QueuedDraw& draw : draws) {
                if (!draw.castsShadows) continue;
                if (Model* model = c->manager.get(draw.model)) model->drawDepthOnly(cmd, *opaque, *masked, draw.matrix, bound);
            }
            vkCmdEndRendering(cmd);
        }
        transition(cmd, image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT, cascadeCount);
    }

    bool ShadowMaps::ready() const {
        auto c = context();
        return image != VK_NULL_HANDLE && c->manager.get(depthOnly) && c->manager.get(alphaMasked);
    }

    void ShadowMaps::destroy() {
        auto c = context();
        if (!c) return;
        VkDevice device = c->vulkan.device;
        if (compareSampler) vkDestroySampler(device, compareSampler, nullptr);
        for (VkImageView& view : layerViews) {
            if (view) vkDestroyImageView(device, view, nullptr);
            view = VK_NULL_HANDLE;
        }
        if (array) vkDestroyImageView(device, array, nullptr);
        if (image) vmaDestroyImage(c->vulkan.allocator, image, allocation);
        compareSampler = VK_NULL_HANDLE;
        array = VK_NULL_HANDLE;
        image = VK_NULL_HANDLE;
        allocation = nullptr;
        size = layers = 0;
        // The pipelines are in the resource manager: destroyAll() frees them.
    }
}
