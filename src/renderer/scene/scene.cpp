#include "scene/scene.h"

#include <algorithm>

#include "core/context.h"
#include "lighting/lightingMath.h"

namespace NRE {
    void Scene::addInstance(Handel<Instance> instance) {
        if (std::ranges::find(instances, instance) == instances.end()) instances.push_back(instance);
    }

    void Scene::removeInstance(Handel<Instance> instance) {
        std::erase(instances, instance);
    }

    void Scene::addLight(Handel<Light> light) {
        if (std::ranges::find(lights, light) == lights.end()) lights.push_back(light);
    }

    void Scene::removeLight(Handel<Light> light) {
        std::erase(lights, light);
    }

    void applyLightingDefaults(Scene& scene) {
        switch (scene.lighting) {
            case SceneLighting::Small: scene.shadowDistance = 20.0f; break;
            case SceneLighting::Landscape: scene.shadowDistance = 500.0f; break;
        }
    }

    Result<void> Scene::writeFrameData() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        uint32_t frame = c->vulkan.currentFrame % max_frames_in_flight;
        gpuLights.clear();
        for (Handel<Light> handle : lights) {
            if (Light* light = c->manager.get(handle)) gpuLights.push_back(light->toGpu());
        }
        FrameData fd{};
        fd.viewProj = c->vulkan.projection * camera.viewMatrix();
        fd.cameraPos = glm::vec4(camera.position, 1.0f);
        fd.lightCount = uint32_t(gpuLights.size());
        fd.sunDirection = glm::vec4(sunDirection, 0.0f);
        fd.sunColor = glm::vec4(sunColor, sunIntensity);
        Handel<Cubemap> sky = c->manager.get(skybox) ? skybox : Handel<Cubemap>{};
        // Sun shadows (3D, with a sun): cascades splitting the view from the near plane to the shadow distance.
        const ShadowMaps& shadows = c->vulkan.shadowMaps;
        const VkExtent2D extent = sceneExtent(c);
        shadowCascades = 0;
        for (glm::mat4& m : fd.cascadeViewProj) m = glm::mat4(1.0f);
        fd.cascadeSplits = glm::vec4(0.0f);
        // No sun (intensity 0), no sun shadows: the shadow pass is skipped.
        if (c->config.mode == RenderMode::Render_3D && sunIntensity > 0.0f && shadows.ready()
            && extent.width > 0 && extent.height > 0) {
            shadowCascades = std::min(LightingMath::cascadeCountFor(shadowDistance), shadows.layerCount());
        }
        if (shadowCascades > 0) {
            const float aspect = float(extent.width) / float(extent.height);
            const glm::mat4 view = camera.viewMatrix();
            const auto splits = LightingMath::cascadeSplits(Camera::nearPlane, shadowDistance, shadowCascades);
            float sliceNear = Camera::nearPlane;
            for (uint32_t i = 0; i < shadowCascades; ++i) {
                auto corners = LightingMath::frustumSliceCorners(view, Camera::fovY, aspect, sliceNear, splits[i]);
                fd.cascadeViewProj[i] = LightingMath::cascadeViewProj(corners, sunDirection, shadows.resolution());
                // nreSunShadow blends cascade i into the next over the last 10% of cascade i, so the next one starts
                // there (the same band as in the shader).
                sliceNear = LightingMath::cascadeBlendStart(splits, i);
            }
            fd.cascadeSplits = glm::vec4(splits[0], splits[1], splits[2], splits[3]);
        }
        fd.shadowParams = glm::vec4(float(shadowCascades), ShadowMaps::normalOffsetTexels, 0.0f, 0.0f);
        // Neutral until the atmosphere, IBL and haze passes fill them: the plain sun colour, the sky colour as
        // linear light (the same expansion the clear colour gets, without the exposure).
        fd.sunRadiance = glm::vec4(sunColor * sunIntensity, sunIntensity > 0.0f ? 1.0f : 0.0f);
        fd.skyColor = glm::vec4(LightingMath::inverseToneMapACES(skyColor), 0.0f);
        fd.skyParams = glm::vec4(sky == Handel<Cubemap>{} ? 0.0f : 1.0f, 0.0f, 0.0f, 0.0f);
        fd.hazeParams = glm::vec4(0.0f);
        if (auto r = UBO.write(frame, &fd, sizeof(fd)); !r) return r;

        // The caller waited on this frame's fence, so its set is free to rewrite.
        if (sky != skyboxInSet[frame]) {
            Cubemap* cubemap = c->manager.get(sky == Handel<Cubemap>{} ? c->vulkan.defaultCubemap : sky);
            if (auto r = writeImageDescriptor(frameSets[frame], 2, cubemap->getView(), c->vulkan.sampler); !r) return r;
            skyboxInSet[frame] = sky;
        }
        return Lights.write(frame, gpuLights.data(), gpuLights.size() * sizeof(GpuLight));
    }

    void Scene::bindFrameData(VkCommandBuffer commandBuffer, const Pipeline& pipeline) {
        uint32_t frame = context()->vulkan.currentFrame % max_frames_in_flight;
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipelineLayout, 0, 1, &frameSets[frame], 0, nullptr);
    }

    void Scene::appendDraws(std::vector<QueuedDraw>& out) const {
        auto c = context();
        for (Handel<Instance> handle : instances) {
            Instance* instance = c->manager.get(handle);
            if (!instance || !instance->visible) continue;
            QueuedDraw& draw = out.emplace_back();
            draw.model = instance->model;
            draw.matrix = instance->transform.matrix();
            draw.castsShadows = instance->castsShadows;
        }
    }

    Result<void> Scene::drawSkybox(VkCommandBuffer commandBuffer) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        if (c->config.mode != RenderMode::Render_3D || !c->manager.get(skybox)) return {};
        Pipeline* pipeline = c->manager.get(c->vulkan.skyboxPipeline);
        if (!pipeline) return {};   // init() failed to make it under RenderErrorPolicy::Ignore, and said so
        uint32_t frame = c->vulkan.currentFrame % max_frames_in_flight;

        // Maps (ndc.x, ndc.y, 1) to the world-space direction through that pixel: undo the projection's scale
        // (x / P[0][0], y / P[1][1], looking down -z), then the camera's rotation. No translation: the sky is
        // infinitely far away, so moving never brings it closer.
        const glm::mat4& p = c->vulkan.projection;
        glm::mat3 unproject(glm::vec3(1.0f / p[0][0], 0.0f, 0.0f), glm::vec3(0.0f, 1.0f / p[1][1], 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
        glm::mat3 cameraToWorld = glm::transpose(glm::mat3(camera.viewMatrix()));
        // Matches the push constant block in shaders/skybox.vert.
        struct { glm::mat4 rayMatrix; glm::vec4 params; } pc{glm::mat4(cameraToWorld * unproject), glm::vec4(1.0f / exposure, 0.0f, 0.0f, 0.0f)};

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
        vkCmdSetCullMode(commandBuffer, VK_CULL_MODE_NONE);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout, 0, 1, &frameSets[frame], 0, nullptr);
        vkCmdPushConstants(commandBuffer, pipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        return {};
    }

    Result<void> Scene::createUboAndLights() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);

        for (VkDescriptorSet& set: frameSets) {
            auto s = c->vulkan.descriptorPool.allocateDescriptorSet(c->vulkan.descriptorLayouts[0]);
            if (!s) return std::unexpected(s.error());
            set = *s;
        }

        auto err = UBO.create(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,sizeof(FrameData));
        if (!err) return err;
        err = Lights.create(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sizeof(GpuLight));
        if (!err) return err;
        err = UBO.attach(frameSets,0);
        if (!err) return err;
        err = Lights.attach(frameSets,1);
        if (!err) return err;
        // Binding 2 always points at a cubemap, so shaders may sample it whether or not there's a skybox.
        Cubemap* defaultCubemap = c->manager.get(c->vulkan.defaultCubemap);
        for (VkDescriptorSet set : frameSets) {
            err = writeImageDescriptor(set, 2, defaultCubemap->getView(), c->vulkan.sampler);
            if (!err) return err;
        }
        // Binding 3: the sun's shadow maps, shared by every scene. Missing only if init() failed to make them under
        // RenderErrorPolicy::Ignore; the cascade count then stays 0, so shaders never sample it.
        const ShadowMaps& shadows = c->vulkan.shadowMaps;
        if (shadows.arrayView() != VK_NULL_HANDLE) {
            for (VkDescriptorSet set : frameSets) {
                err = writeImageDescriptor(set, 3, shadows.arrayView(), shadows.sampler());
                if (!err) return err;
            }
        }
        return {};
    }
}
