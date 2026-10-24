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
    Result<void> Scene::draw(VkCommandBuffer commandBuffer) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
        auto pipeline = c->manager.get(c->vulkan.pipelineBound);
        if (!pipeline) {
            if (ErrorHandeling::handleError(Error{ErrorCode::NoPipelineBound,"No pipeline was bound. Bind one with bindpipeline."})) {
                return std::unexpected(Error{ErrorCode::NoPipelineBound,"No pipeline was bound. Bind one with bindpipeline."});
            };
        }
        auto viewProjection = c->vulkan.projection * camera.viewMatrix();
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
        for (Handel<Instance> instanceHandel : instances) {
            Instance* instance = c->manager.get(instanceHandel);
            if (!instance || !instance->visible) continue;
            Model* model = c->manager.get(instance->model);
            if (!model) continue;
            (void)model->draw(commandBuffer,*pipeline,instance->transform.matrix(),viewProjection);
        }
        return {};
    }
}
