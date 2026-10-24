#pragma once
#include <cmath>
#include <vector>

#include "gpu/buffer.h"
#include "renderer.h"

namespace NRE {

    struct Camera {
        static constexpr glm::vec3 worldUp{0.0f, 1.0f, 0.0f};
        static constexpr float maxPitch = 1.5533430f;

        glm::vec3 position{0.0f, 0.0f, 3.0f};
        float yaw = 0.0f;     // radians, positive turns right
        float pitch = 0.0f;   // radians, positive looks up

        glm::vec3 forward() const {
            return {std::cos(pitch) * std::sin(yaw), std::sin(pitch), -std::cos(pitch) * std::cos(yaw)};
        }
        glm::vec3 right() const { return glm::normalize(glm::cross(forward(), worldUp)); }
        glm::vec3 up() const { return glm::cross(right(), forward()); }

        void rotate(float yawDelta, float pitchDelta) {
            yaw += yawDelta;
            pitch = glm::clamp(pitch + pitchDelta, -maxPitch, maxPitch);
        }
        // Turns the camera to face target. Does nothing if target is the camera's own position.
        void lookAt(glm::vec3 target) {
            glm::vec3 dir = target - position;
            if (glm::length(dir) < 1e-6f) return;
            dir = glm::normalize(dir);
            yaw = std::atan2(dir.x, -dir.z);
            pitch = glm::clamp(std::asin(dir.y), -maxPitch, maxPitch);
        }

        // World space -> camera space. The projection goes on the left: clip = projection * view * model.
        glm::mat4 viewMatrix() const {
            return glm::lookAt(position, position + forward(), worldUp);
        }
    };
    class Scene {
        std::vector<Handel<Instance>> instances;
    public:
        Camera camera;
        // Does nothing if the instance is already in the scene.
        void addInstance(Handel<Instance> instance);
        void removeInstance(Handel<Instance> instance);
        const std::vector<Handel<Instance>>& getInstances() const { return instances; }
        Result<void> draw(VkCommandBuffer commandBuffer);
    };
}
