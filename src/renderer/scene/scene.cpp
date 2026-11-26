#include "scene/scene.h"

#include <algorithm>

#include "core/context.h"

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
    Result<void> Scene::bindFrameData(VkCommandBuffer commandBuffer) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        auto pipeline = c->manager.get(c->vulkan.pipelineBound);
        if (!pipeline) {
            auto error = Error{ErrorCode::NoPipelineBound, "Scene::bindFrameData(): no pipeline is bound. "
                               "Create one with createPipeline() and bind it with bindPipeline()."};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            return {};
        }
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
        if (auto r = UBO.write(frame, &fd, sizeof(fd)); !r) return r;

        // The caller waited on this frame's fence, so its set is free to rewrite.
        Handel<Cubemap> sky = c->manager.get(skybox) ? skybox : Handel<Cubemap>{};
        if (sky != skyboxInSet[frame]) {
            Cubemap* cubemap = c->manager.get(sky == Handel<Cubemap>{} ? c->vulkan.defaultCubemap : sky);
            if (auto r = writeImageDescriptor(frameSets[frame], 2, cubemap->getView(), c->vulkan.sampler); !r) return r;
            skyboxInSet[frame] = sky;
        }
        if (auto r = Lights.write(frame, gpuLights.data(), gpuLights.size() * sizeof(GpuLight)); !r) return r;
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout,0,1,&frameSets[frame],0,nullptr);
        return {};
    }

    Result<void> Scene::draw(VkCommandBuffer commandBuffer) {
        if (auto r = bindFrameData(commandBuffer); !r) return r;
        auto c = context();
        Pipeline* pipeline = c->manager.get(c->vulkan.pipelineBound);
        if (!pipeline) return {};   // bindFrameData already reported it (RenderErrorPolicy::Ignore)
        for (Handel<Instance> instanceHandel : instances) {
            Instance* instance = c->manager.get(instanceHandel);
            if (!instance || !instance->visible) continue;
            Model* model = c->manager.get(instance->model);
            if (!model) continue;
            (void)model->draw(commandBuffer,*pipeline,instance->transform.matrix());
        }
        return {};
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
        PushConstants pc{glm::mat4(cameraToWorld * unproject), glm::mat4(1.0f)};

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout, 0, 1, &frameSets[frame], 0, nullptr);
        vkCmdPushConstants(commandBuffer, pipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &pc);
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
        return {};
    }
}
