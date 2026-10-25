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
    Result<void> Scene::draw(VkCommandBuffer commandBuffer) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        auto pipeline = c->manager.get(c->vulkan.pipelineBound);
        if (!pipeline) {
            auto error = Error{ErrorCode::NoPipelineBound, "Scene::draw(): no pipeline is bound. "
                               "Create one with createPipeline() and bind it with bindPipeline()."};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            return {};
        }
        uint32_t frame = c->vulkan.currentFrame % max_frames_in_flight;
        gpuLights.clear();
        for (Handel<Light> handle : lights) {
            if (Light* light = c->manager.get(handle)) gpuLights.push_back(light->toGpu());
        }
        FrameData fd{c->vulkan.projection * camera.viewMatrix(), glm::vec4(camera.position, 1.0f), uint32_t(gpuLights.size())};
        if (auto r = UBO.write(frame, &fd, sizeof(fd)); !r) return r;
        if (auto r = Lights.write(frame, gpuLights.data(), gpuLights.size() * sizeof(GpuLight)); !r) return r;
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout,0,1,&frameSets[frame],0,nullptr);
        for (Handel<Instance> instanceHandel : instances) {
            Instance* instance = c->manager.get(instanceHandel);
            if (!instance || !instance->visible) continue;
            Model* model = c->manager.get(instance->model);
            if (!model) continue;
            (void)model->draw(commandBuffer,*pipeline,instance->transform.matrix());
        }
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
        return Lights.attach(frameSets,1);
    }
}
