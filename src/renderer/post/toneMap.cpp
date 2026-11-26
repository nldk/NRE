#include "post/toneMap.h"

#include <span>

#include "core/context.h"
#include "core/errorHandling.h"

namespace NRE {
    Result<void> ToneMapPass::create() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        static constexpr uint32_t fullscreenVert[] = {
#include "embeddedShaders/fullscreen.vert.inc"
        };
        static constexpr uint32_t tonemapFrag[] = {
#include "embeddedShaders/tonemap.frag.inc"
        };
        Handel<Shader> vertex = c->manager.create(Shader(std::span(fullscreenVert), ShaderType::VERTEX_SHADER));
        Handel<Shader> fragment = c->manager.create(Shader(std::span(tonemapFrag), ShaderType::FRAGMENT_SHADER));
        // No vertex input, no culling, and depth ignored: the output pass has a depth attachment (the unsafe
        // frame callback draws in the same pass), so the pipeline declares one but never tests or writes it.
        Pipeline toneMap;
        auto err = toneMap.createPipeline(vertex, fragment, {}, VK_CULL_MODE_NONE, VK_COMPARE_OP_ALWAYS, false,
                                          c->vulkan.swapchain.image_format);
        c->manager.destroy(vertex, true);   // the pipeline doesn't need the modules any more
        c->manager.destroy(fragment, true);
        if (!err) return err;
        if (toneMap.pipeline != VK_NULL_HANDLE) pipeline = c->manager.create(std::move(toneMap));

        auto s = c->vulkan.descriptorPool.allocateDescriptorSet(c->vulkan.descriptorLayouts[1]);   // the material layout
        if (!s) return std::unexpected(s.error());
        set = *s;
        return {};
    }

    Result<void> ToneMapPass::resize(VkExtent2D extent) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        VkExtent2D current = hdr.getExtent();
        if (hdr.getImage() != VK_NULL_HANDLE && current.width == extent.width && current.height == extent.height) return {};
        if (extent.width == 0 || extent.height == 0) return {};   // minimized window: keep the old one
        Image image;
        auto created = image.createImage(extent.width, extent.height, hdrFormat,
                                         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 1);
        // Under RenderErrorPolicy::Ignore a failed createImage returns success with no image; keep the old one then.
        if (!created || image.getImage() == VK_NULL_HANDLE) return created;
        if (set != VK_NULL_HANDLE) {
            if (auto written = writeImageDescriptor(set, 0, image.getView(), c->vulkan.sampler); !written) return written;
        }
        hdr = std::move(image);   // frees the old image
        return {};
    }

    void ToneMapPass::record(VkCommandBuffer commandBuffer, float exposure, bool passthrough2D) {
        auto c = context();
        Pipeline* p = c->manager.get(pipeline);
        if (!p || set == VK_NULL_HANDLE) return;   // create() failed under RenderErrorPolicy::Ignore, and said so
        glm::vec4 params{exposure, passthrough2D ? 1.0f : 0.0f, 0.0f, 0.0f};
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, p->pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, p->pipelineLayout, 1, 1, &set, 0, nullptr);
        vkCmdPushConstants(commandBuffer, p->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(params), &params);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    }

    void ToneMapPass::destroy() {
        (void)hdr.destroyImage();
        set = VK_NULL_HANDLE;   // freed with the descriptor pool
    }
}
