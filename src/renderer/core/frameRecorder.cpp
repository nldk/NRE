#include "core/frameRecorder.h"

#include "core/context.h"
#include "gpu/upload.h"
#include "lighting/lightingMath.h"

namespace NRE {
    namespace {
        // Starts dynamic rendering into colorView (cleared) + depthView and sets a viewport/scissor covering extent.
        void beginRendering(VkCommandBuffer cmd, VkImageView colorView, VkImageView depthView, VkExtent2D extent,
                            glm::vec3 clearColor, VkAttachmentLoadOp depthLoad = VK_ATTACHMENT_LOAD_OP_CLEAR,
                            VkAttachmentStoreOp depthStore = VK_ATTACHMENT_STORE_OP_DONT_CARE) {
            VkRenderingAttachmentInfo color{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
            color.imageView = colorView;
            color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            color.clearValue.color = {{clearColor.r, clearColor.g, clearColor.b, 1.0f}};
            VkRenderingAttachmentInfo depth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
            depth.imageView = depthView;
            depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depth.loadOp = depthLoad;
            depth.storeOp = depthStore;
            depth.clearValue.depthStencil = {0.0f, 0};   // reverse-Z: far is 0
            VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
            ri.renderArea = {{0, 0}, extent};
            ri.layerCount = 1;
            ri.colorAttachmentCount = 1;
            ri.pColorAttachments = &color;
            ri.pDepthAttachment = &depth;
            vkCmdBeginRendering(cmd, &ri);

            VkViewport viewport{0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f};
            VkRect2D scissor{{0, 0}, extent};
            vkCmdSetViewport(cmd, 0, 1, &viewport);
            vkCmdSetScissor(cmd, 0, 1, &scissor);
        }
    }

    Result<void> recordFrame(Context* c, Scene* scene, std::span<const QueuedDraw> draws) {
        VulkanContext& v = c->vulkan;
        VkCommandBuffer cmd = v.frames[v.currentFrame % max_frames_in_flight].cmd;
        vkResetCommandBuffer(cmd, 0);
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &bi);

        const bool is3D = c->config.mode == RenderMode::Render_3D;
        const float exposure = scene ? scene->frameExposure : 1.0f;   // setExposure x the automatic part (Landscape)
        const glm::vec3 sky = scene ? scene->skyColor : Scene::defaultSkyColor;
        // The sky colour is a display colour: expanded with the inverse tone curve and divided by the exposure, the
        // tone map turns it back into itself. 2D isn't tone mapped, so it's used as it is.
        const glm::vec3 clear = is3D ? LightingMath::inverseToneMapACES(sky) / exposure : sky;

        // Where the tone-mapped scene goes: the window, or (unsafe API) the render target, which the frame
        // callback then shows on the window. The scene's depth buffer is the one of the same size.
        VkImage windowImage = v.images[v.currentImageIndex];
        VkImageView windowView = v.views[v.currentImageIndex];
        VkImage outImage = windowImage, depthImage = v.depth.image;
        VkImageView outView = windowView, depthView = v.depth.view;
        bool toTarget = false;
#ifdef NRE_UNSAFE_API
        if (const UnsafeContext& u = c->unsafeContext; u.targetColor.getImage() != VK_NULL_HANDLE) {
            outImage = u.targetColor.getImage();
            outView = u.targetColor.getView();
            depthImage = u.targetDepth.getImage();
            depthView = u.targetDepth.getView();
            toTarget = true;
        }
#endif
        const VkExtent2D extent = sceneExtent(c);
        ToneMapPass& toneMap = v.toneMap;
        // Only missing (or the wrong size) after a failed resize under RenderErrorPolicy::Ignore: the frame is then empty.
        const VkExtent2D hdrExtent = toneMap.hdrExtent();
        const bool haveHdr = toneMap.hdrImage() != VK_NULL_HANDLE && hdrExtent.width == extent.width && hdrExtent.height == extent.height;

        // The procedural sky's cubemap for this frame slot, recomputed if the sun moved (compute, then a barrier to
        // the fragment shaders that read it). Before the main pass, which samples it (sky pass, ambient).
        if (haveHdr && scene && scene->skySource == SkySource::Procedural) v.sky.update(cmd, scene->sunDirection, scene->skyBrightness);
        // The sun's shadow maps, from the same draws (writeFrameData() set the cascades). Before the main pass, which
        // samples them.
        if (haveHdr && scene) v.shadowMaps.record(cmd, *scene, draws, scene->shadowCascades);

        // Last frame's contents aren't needed (both are cleared), so UNDEFINED lets the driver discard them.
        transition(cmd, depthImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
        if (haveHdr) {
            // Main pass. Without a render target the window's depth is kept: the frame callback draws in the
            // tone-map pass, and used to see the scene's depth.
            transition(cmd, toneMap.hdrImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            beginRendering(cmd, toneMap.hdrView(), depthView, extent, clear, VK_ATTACHMENT_LOAD_OP_CLEAR,
                           toTarget ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE);
            Pipeline* pipeline = c->manager.get(v.framePipeline);
            if (scene && pipeline) {
                scene->bindFrameData(cmd, *pipeline);
                VkCullModeFlags cullMode = Model::unknownCullMode;   // set by the first mesh
                for (const QueuedDraw& draw : draws) {
                    if (Model* model = c->manager.get(draw.model)) (void)model->draw(cmd, *pipeline, draw.matrix, cullMode);
                }
                // After the geometry, so the sky only fills what it left empty.
                if (is3D) v.sky.draw(cmd, *scene);
            }
            vkCmdEndRendering(cmd);
            transition(cmd, toneMap.hdrImage(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            // Same layout: only makes the main pass's depth writes visible to the next pass.
            transition(cmd, depthImage, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
        }

        // Tone map into the output. Every pixel is written, so the clear colour never shows.
        transition(cmd, outImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        beginRendering(cmd, outView, depthView, extent, glm::vec3(0.0f),
                       toTarget || !haveHdr ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD);
        if (haveHdr) toneMap.record(cmd, exposure, !is3D);
#ifdef NRE_UNSAFE_API
        if (toTarget) {
            // A pass on the window: the target is now a texture the frame callback can sample (e.g. ImGui::Image).
            vkCmdEndRendering(cmd);
            transition(cmd, outImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            transition(cmd, windowImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            transition(cmd, v.depth.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
            beginRendering(cmd, windowView, v.depth.view, v.swapchain.extent, glm::vec3(0.05f));
        }
        if (auto callback = c->unsafeContext.frameCallback) callback(cmd);
#endif
        vkCmdEndRendering(cmd);
        transition(cmd, windowImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        vkEndCommandBuffer(cmd);
        return {};
    }
}
