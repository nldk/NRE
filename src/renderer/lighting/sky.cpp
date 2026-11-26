#include "lighting/sky.h"

#include <cmath>
#include <string>

#include "core/context.h"
#include "core/errorHandling.h"
#include "gpu/upload.h"
#include "pipeline/shader.h"
#include "scene/scene.h"

namespace NRE {
    namespace {
        constexpr VkFormat skyFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        constexpr uint32_t groupSize = 8;   // local_size_x/y of both compute shaders

        // Matches the push constant block of skyCubemap.comp.
        struct CubemapPush { glm::vec4 toSun; };
        // Matches the push constant block of sky.vert.
        struct DrawPush { glm::mat4 rayMatrix; glm::vec4 params; };

        void imageBarrier(VkCommandBuffer cmd, VkImage image, uint32_t baseMip, uint32_t mipCount, uint32_t layers,
                          VkImageLayout from, VkImageLayout to,
                          VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
                          VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess) {
            VkImageMemoryBarrier2 b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
            b.srcStageMask = srcStage;
            b.srcAccessMask = srcAccess;
            b.dstStageMask = dstStage;
            b.dstAccessMask = dstAccess;
            b.oldLayout = from;
            b.newLayout = to;
            b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image = image;
            b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, baseMip, mipCount, 0, layers};
            VkDependencyInfo d{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            d.imageMemoryBarrierCount = 1;
            d.pImageMemoryBarriers = &b;
            vkCmdPipelineBarrier2(cmd, &d);
        }

        // Angle between two unit vectors, precise for small angles (acos isn't).
        float angleBetween(glm::vec3 a, glm::vec3 b) {
            return 2.0f * std::asin(std::min(0.5f * glm::length(a - b), 1.0f));
        }
    }

    Result<void> Sky::create() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        destroy();

        // The draw pass first: cubemap skies only need it, so they still work if the procedural parts fail.
        {
            static constexpr uint32_t skyVert[] = {
#include "embeddedShaders/sky.vert.inc"
            };
            static constexpr uint32_t skyFrag[] = {
#include "embeddedShaders/sky.frag.inc"
            };
            BuiltinShader vertex(skyVert, ShaderType::VERTEX_SHADER);
            BuiltinShader fragment(skyFrag, ShaderType::FRAGMENT_SHADER);
            // No vertex input (the vertex shader makes the triangle) and no culling (draw() sets it). The sky sits at
            // depth 0, the cleared value (reverse-Z), so GREATER_OR_EQUAL passes only where nothing was drawn, and it
            // doesn't write depth.
            Pipeline pipeline;
            if (auto r = pipeline.createPipeline(vertex.handle, fragment.handle, {}, VK_COMPARE_OP_GREATER_OR_EQUAL, false); !r) return r;
            if (pipeline.pipeline != VK_NULL_HANDLE) drawPipeline = c->manager.create(std::move(pipeline));
        }

        // Linear and clamped: the LUT's edges are its ranges' ends, nothing wraps.
        VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sci.magFilter = sci.minFilter = VK_FILTER_LINEAR;
        sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sci.maxLod = 0.0f;
        if (VkResult r = vkCreateSampler(c->vulkan.device, &sci, nullptr, &lutSampler); r != VK_SUCCESS) {
            lutSampler = VK_NULL_HANDLE;
            auto error = Error{ErrorCode::ErrorCreatingTexture, "init(): failed to create the sky's LUT sampler: "
                               + std::string(ErrorHandeling::vkResultName(r))};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            return {};   // no procedural sky
        }

        static constexpr uint32_t transmittanceLutComp[] = {
#include "embeddedShaders/transmittanceLut.comp.inc"
        };
        static constexpr uint32_t skyCubemapComp[] = {
#include "embeddedShaders/skyCubemap.comp.inc"
        };
        const VkDescriptorSetLayoutBinding lutBindings[] = {
            {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };
        const VkDescriptorSetLayoutBinding cubemapBindings[] = {
            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };
        if (auto r = lutPass.create(transmittanceLutComp, lutBindings, 0); !r) return r;
        if (auto r = cubemapPass.create(skyCubemapComp, cubemapBindings, sizeof(CubemapPush)); !r) return r;
        if (!lutPass.ready() || !cubemapPass.ready()) return {};   // RenderErrorPolicy::Ignore: already reported

        if (auto r = transmittance.createImage(lutWidth, lutHeight, skyFormat, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 1); !r) return r;
        if (transmittance.getImage() == VK_NULL_HANDLE) return {};
        for (Cubemap& cubemap : cubemaps) {
            if (auto r = cubemap.createStorage(cubemapSize, skyFormat, 0); !r) return r;
            if (cubemap.getImage() == VK_NULL_HANDLE) return {};
        }

        auto lutSet = lutPass.allocateSet();
        if (!lutSet) return std::unexpected(lutSet.error());
        if (auto r = writeStorageImageDescriptor(*lutSet, 0, transmittance.getMipView(0)); !r) return r;
        for (uint32_t slot = 0; slot < max_frames_in_flight; ++slot) {
            auto set = cubemapPass.allocateSet();
            if (!set) return std::unexpected(set.error());
            cubemapSets[slot] = *set;
            if (auto r = writeImageDescriptor(*set, 0, transmittance.getView(), lutSampler); !r) return r;
            if (auto r = writeStorageImageDescriptor(*set, 1, cubemaps[slot].getMipView(0)); !r) return r;
        }

        // The LUT depends on nothing but the constants: computed once, here.
        return immediateSubmit([&](VkCommandBuffer cmd) {
            imageBarrier(cmd, transmittance.getImage(), 0, 1, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                         VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
            lutPass.dispatch(cmd, *lutSet, nullptr, (lutWidth + groupSize - 1) / groupSize, (lutHeight + groupSize - 1) / groupSize, 1);
            imageBarrier(cmd, transmittance.getImage(), 0, 1, 1, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                         VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
        });
    }

    bool Sky::update(VkCommandBuffer cmd, glm::vec3 sunDirection) {
        if (!ready()) return false;
        auto c = context();
        const uint32_t slot = c->vulkan.currentFrame % max_frames_in_flight;
        const float length = glm::length(sunDirection);
        const glm::vec3 toSun = length > 0.0f ? -sunDirection / length : glm::vec3(0.0f, -1.0f, 0.0f);
        if (!generated || angleBetween(toSun, generatedSun) > regenerateAngle) {
            generatedSun = toSun;
            generated = true;
        }
        if (slotValid[slot] && slotSun[slot] == generatedSun) return false;
        slotSun[slot] = generatedSun;
        slotValid[slot] = true;

        const Cubemap& cubemap = cubemaps[slot];
        const VkImage image = cubemap.getImage();
        const uint32_t mips = cubemap.getMipLevels();
        // Mip 0 is rewritten whole, so its old contents go (UNDEFINED). The last reads of this cubemap were by this
        // slot's previous frame, which has finished (its fence was waited on before recording).
        imageBarrier(cmd, image, 0, 1, 6, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                     VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
        CubemapPush push{glm::vec4(generatedSun, 0.0f)};
        const uint32_t groups = (cubemapSize + groupSize - 1) / groupSize;
        cubemapPass.dispatch(cmd, cubemapSets[slot], &push, groups, groups, 6);

        // The mips, each a linear 2x2 average of the one above, by blits. Mip 0 becomes the first source; the others
        // are written whole.
        imageBarrier(cmd, image, 0, 1, 6, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                     VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                     VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
        if (mips > 1) {
            imageBarrier(cmd, image, 1, mips - 1, 6, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
        }
        for (uint32_t level = 1; level < mips; ++level) {
            const int32_t src = int32_t(std::max(cubemapSize >> (level - 1), 1u)), dst = int32_t(std::max(cubemapSize >> level, 1u));
            VkImageBlit2 region{VK_STRUCTURE_TYPE_IMAGE_BLIT_2};
            region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 0, 6};
            region.srcOffsets[1] = {src, src, 1};
            region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 6};
            region.dstOffsets[1] = {dst, dst, 1};
            VkBlitImageInfo2 blit{VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2};
            blit.srcImage = image;
            blit.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            blit.dstImage = image;
            blit.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            blit.regionCount = 1;
            blit.pRegions = &region;
            blit.filter = VK_FILTER_LINEAR;
            vkCmdBlitImage2(cmd, &blit);
            imageBarrier(cmd, image, level, 1, 6, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                         VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
        }
        // Readable by the main pass (sky.frag and the lighting's ambient) and by later compute passes (the IBL filter).
        imageBarrier(cmd, image, 0, mips, 6, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                     VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
        return true;
    }

    void Sky::draw(VkCommandBuffer cmd, Scene& scene) {
        auto c = context();
        if (c->config.mode != RenderMode::Render_3D || scene.skySource == SkySource::Color) return;
        Pipeline* pipeline = c->manager.get(drawPipeline);
        if (!pipeline) return;   // create() failed to make it under RenderErrorPolicy::Ignore, and said so

        // Maps (ndc.x, ndc.y, 1) to the world-space direction through that pixel: undo the projection's scale
        // (x / P[0][0], y / P[1][1], looking down -z), then the camera's rotation. No translation: the sky is
        // infinitely far away, so moving never brings it closer.
        const glm::mat4& p = c->vulkan.projection;
        glm::mat3 unproject(glm::vec3(1.0f / p[0][0], 0.0f, 0.0f), glm::vec3(0.0f, 1.0f / p[1][1], 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
        glm::mat3 cameraToWorld = glm::transpose(glm::mat3(scene.camera.viewMatrix()));
        // A pixel at the centre of the view spans 2 / (P[1][1] * height) radians: the sun disk's edge is blended over it.
        const VkExtent2D extent = sceneExtent(c);
        const float pixelAngle = extent.height > 0 ? 2.0f / (std::fabs(p[1][1]) * float(extent.height)) : 0.0f;
        DrawPush push{glm::mat4(cameraToWorld * unproject), glm::vec4(1.0f / scene.frameExposure, pixelAngle, 0.0f, 0.0f)};

        scene.bindFrameData(cmd, *pipeline);
        vkCmdSetCullMode(cmd, VK_CULL_MODE_NONE);
        vkCmdPushConstants(cmd, pipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);
        vkCmdDraw(cmd, 3, 1, 0, 0);
    }

    VkImageView Sky::cubemapView() const {
        auto c = context();
        return cubemaps[c->vulkan.currentFrame % max_frames_in_flight].getView();
    }

    bool Sky::ready() const {
        auto c = context();
        return c && c->manager.get(drawPipeline) && lutPass.ready() && cubemapPass.ready() && lutSampler != VK_NULL_HANDLE
               && transmittance.getImage() != VK_NULL_HANDLE && cubemaps[max_frames_in_flight - 1].getImage() != VK_NULL_HANDLE
               && cubemapSets[max_frames_in_flight - 1] != VK_NULL_HANDLE;
    }

    void Sky::destroy() {
        auto c = context();
        if (!c) return;
        lutPass.destroy();
        cubemapPass.destroy();
        (void)transmittance.destroyImage();
        for (Cubemap& cubemap : cubemaps) cubemap = Cubemap{};
        if (lutSampler) vkDestroySampler(c->vulkan.device, lutSampler, nullptr);
        lutSampler = VK_NULL_HANDLE;
        for (VkDescriptorSet& set : cubemapSets) set = VK_NULL_HANDLE;   // freed with the descriptor pool
        generated = false;
        for (bool& valid : slotValid) valid = false;
        // The draw pipeline is in the resource manager: destroyAll() frees it.
    }
}
