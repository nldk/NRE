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
            case SceneLighting::Small:
                scene.shadowDistance = 20.0f;
                scene.proceduralSky = false;
                scene.hazeVisibility = 0.0f;
                scene.hazeHeight = 300.0f;
                break;
            case SceneLighting::Landscape:
                scene.shadowDistance = 500.0f;
                scene.proceduralSky = true;
                scene.hazeVisibility = 8000.0f;
                scene.hazeHeight = 300.0f;
                break;
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
        const bool is3D = c->config.mode == RenderMode::Render_3D;
        const bool landscape = lighting == SceneLighting::Landscape;

        // The sky: the skybox if there's one (a destroyed one counts as none), otherwise the mode's default.
        const Handel<Cubemap> sky = c->manager.get(skybox) ? skybox : Handel<Cubemap>{};
        if (sky != Handel<Cubemap>{}) skySource = SkySource::Cubemap;
        else if (proceduralSky && is3D && c->vulkan.sky.ready()) skySource = SkySource::Procedural;
        else skySource = SkySource::Color;

        // The sun's light on objects. Landscape: through the atmosphere, which also fades it out as the disk sinks
        // below the horizon (0 from -0.95 degrees down), and the exposure follows the sun's height.
        const float elevation = LightingMath::sunElevation(sunDirection);
        const glm::vec3 transmittance = landscape ? LightingMath::sunTransmittance(elevation) : glm::vec3(1.0f);
        const glm::vec3 sunLight = sunColor * sunIntensity * transmittance;
        const bool sunUp = sunIntensity > 0.0f && glm::any(glm::greaterThan(transmittance, glm::vec3(0.0f)));
        // No sun at all (intensity 0) is night, wherever its direction points: the night exposure (as at -10 degrees
        // and below), only the night floor in the sky.
        const float exposureElevation = sunIntensity > 0.0f ? elevation : -1.5707963f;
        // Only with the procedural sky: a cubemap sky has a fixed, baked look, so it and its light on objects stay at
        // setExposure, as in Small.
        const bool autoExposure = landscape && skySource == SkySource::Procedural;
        frameExposure = exposure * (autoExposure ? LightingMath::autoExposure(exposureElevation) : 1.0f);

        // Sun shadows (3D, with a sun): cascades splitting the view from the near plane to the shadow distance.
        const ShadowMaps& shadows = c->vulkan.shadowMaps;
        const VkExtent2D extent = sceneExtent(c);
        shadowCascades = 0;
        for (glm::mat4& m : fd.cascadeViewProj) m = glm::mat4(1.0f);
        fd.cascadeSplits = glm::vec4(0.0f);
        // No sun (intensity 0, or set below the horizon), no sun shadows: the shadow pass is skipped.
        if (is3D && sunUp && shadows.ready()
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
        fd.sunRadiance = glm::vec4(sunLight, sunUp ? 1.0f : 0.0f);
        // The sky colour as linear light (the same expansion the clear colour gets, without the exposure).
        fd.skyColor = glm::vec4(LightingMath::inverseToneMapACES(skyColor), 0.0f);
        // z: the procedural sky's scattering brightness: the sunlit sky scales with the sun's intensity (3 is the
        // standard look), the night floor doesn't. Sky::update bakes both into the cubemap.
        skyBrightness = skySource == SkySource::Procedural
                      ? std::max(sunIntensity, 0.0f) / float(LightingMath::Atmosphere::standardIntensity) : 0.0f;
        // The sky's light on objects (nreAmbient): the IBL filtered from this frame's sky, if it has one (the
        // procedural sky always does when it's ready; a skybox once setSkybox filtered it). Without one (the sky
        // colour, or a failed filter) the specular mip count is 0 and nreAmbient uses the sky colour's hemisphere.
        const Ibl* ibl = nullptr;
        if (skySource == SkySource::Procedural) ibl = &c->vulkan.sky.ibl(frame);
        else if (skySource == SkySource::Cubemap) ibl = c->manager.get(sky)->getIbl();
        if (ibl && !ibl->ready()) ibl = nullptr;
        fd.skyParams = glm::vec4(float(uint32_t(skySource)), ibl ? float(ibl->specularMips()) : 0.0f, skyBrightness, 0.0f);
        // Haze (nreApplyHaze, and the procedural sky's horizon in sky.frag): extinction per metre at y = 0 and below,
        // thinning out exponentially above it (1 / e per hazeHeight).
        fd.hazeParams = glm::vec4(LightingMath::hazeDensityForVisibility(hazeVisibility), 1.0f / hazeHeight, 0.0f, 0.0f);
        if (auto r = UBO.write(frame, &fd, sizeof(fd)); !r) return r;

        // The caller waited on this frame's fence, so its set is free to rewrite. The procedural sky of frame slot f
        // is always the Sky's cubemap f, the one only that slot's frames read and write.
        const SkyBinding wanted{skySource == SkySource::Cubemap ? sky : Handel<Cubemap>{}, skySource == SkySource::Procedural,
                                ibl != nullptr};
        if (wanted != skyInSet[frame]) {
            const VkImageView none = c->manager.get(c->vulkan.defaultCubemap)->getView();
            VkImageView view = wanted.procedural ? c->vulkan.sky.cubemapView(frame)
                             : wanted.skybox != Handel<Cubemap>{} ? c->manager.get(wanted.skybox)->getView() : none;
            if (auto r = writeImageDescriptor(frameSets[frame], 2, view, c->vulkan.sampler); !r) return r;
            const VkSampler iblSampler = c->vulkan.iblFilter.ready() ? c->vulkan.iblFilter.sampler() : c->vulkan.sampler;
            if (auto r = writeImageDescriptor(frameSets[frame], 4, ibl ? ibl->irradianceView() : none, iblSampler); !r) return r;
            if (auto r = writeImageDescriptor(frameSets[frame], 5, ibl ? ibl->specularView() : none, iblSampler); !r) return r;
            skyInSet[frame] = wanted;
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
        // Bindings 2, 4 and 5 always point at a cubemap, so shaders may sample them whether or not there's a sky.
        // Binding 6: the BRDF LUT, the same for every frame and scene. Without the IBL passes (they failed under
        // RenderErrorPolicy::Ignore) it shows the white texture; the IBL is then never used, and the hemisphere's
        // reflections come out too bright, but finite.
        Cubemap* defaultCubemap = c->manager.get(c->vulkan.defaultCubemap);
        const IblFilter& ibl = c->vulkan.iblFilter;
        for (VkDescriptorSet set : frameSets) {
            for (uint32_t binding : {2u, 4u, 5u}) {
                err = writeImageDescriptor(set, binding, defaultCubemap->getView(), c->vulkan.sampler);
                if (!err) return err;
            }
            err = ibl.ready() ? writeImageDescriptor(set, 6, ibl.brdfLutView(), ibl.sampler())
                              : writeImageDescriptor(set, 6, c->manager.get(c->vulkan.whiteTexture)->getView(), c->vulkan.sampler);
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
