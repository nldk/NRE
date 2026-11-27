#include "scene/picking.h"

#include "core/context.h"
#include "scene/model.h"
#include "scene/scene.h"

namespace NRE::Picking {
    std::optional<RayHit> pickScene(const Scene& scene, const Ray& ray) {
        auto c = context();
        std::optional<RayHit> best;
        for (Handel<Instance> handle : scene.getInstances()) {
            Instance* instance = c->manager.get(handle);
            if (!instance || !instance->visible) continue;
            Model* model = c->manager.get(instance->model);
            if (!model) continue;

            // Into model space. The direction isn't renormalised, so t stays the distance in world units.
            glm::mat4 matrix = instance->transform.matrix();
            if (!(std::fabs(glm::determinant(matrix)) > 1e-30f)) continue;   // scaled to nothing
            glm::mat4 inverse = glm::inverse(matrix);
            glm::vec3 origin = glm::vec3(inverse * glm::vec4(ray.origin, 1.0f));
            glm::vec3 direction = glm::vec3(inverse * glm::vec4(ray.direction, 0.0f));

            for (const Mesh& mesh : model->meshes) {
                if (mesh.indices.size() < 3 || mesh.vertices.empty()) continue;
                float limit = best ? best->distance : INFINITY;
                if (!rayAabb(origin, direction, mesh.boundsMin, mesh.boundsMax, limit)) continue;
                float t = 0.0f;
                if (rayMesh<Vertex>(origin, direction, mesh.vertices, mesh.indices, t) && t < limit) {
                    best = RayHit{handle, t, ray.origin + ray.direction * t};
                }
            }
        }
        return best;
    }
}
