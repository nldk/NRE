#include "scene.h"

#include <algorithm>

#include "context.h"

namespace NRE {
    void Scene::addModel(Handel<Model> model) {
        if (std::ranges::find(models, model) == models.end()) models.push_back(model);
    }

    void Scene::removeModel(Handel<Model> model) {
        std::erase(models, model);
    }
    Result<void> Scene::draw(VkCommandBuffer commandBuffer) {
        auto c = context();
        if (!c) {
            if (ErrorHandeling::handleError(Error{ErrorCode::NotInitialized,"Renderer not initialized. Initialize with init."})) {
                return std::unexpected(Error{ErrorCode::NotInitialized,"Renderer not initialized. Initialize with init."});
            };
        }
        auto pipeline = c->manager.get(c->vulkan.pipelineBound);
        if (!pipeline) {
            if (ErrorHandeling::handleError(Error{ErrorCode::NoPipelineBound,"No pipeline was bound. Bind one with bindpipeline."})) {
                return std::unexpected(Error{ErrorCode::NoPipelineBound,"No pipeline was bound. Bind one with bindpipeline."});
            };
        }
        auto viewProjection = c->vulkan.projection * camera.viewMatrix();
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
        for (auto& modelHandel : models) {
            auto model = c->manager.get(modelHandel);
            if (!model) {
                continue;
            }
            model->draw(commandBuffer,*pipeline,viewProjection);
        }
        return {};
    }
}
