#pragma once
#include <cmath>
#include <vector>

#include "gpu/buffer.h"
#include "renderer.h"
#include "gpu/UBOAndSSBO.h"

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
        std::vector<Handel<Light>> lights;
        std::vector<GpuLight> gpuLights;   // rebuilt from lights every frame; a member so it keeps its capacity
        VkDescriptorSet frameSets[max_frames_in_flight]{};
        PerFrameBuffer UBO;
        PerFrameBuffer Lights;
        // What binding 2 of each frame's set points at; empty = the default (black) cubemap. Kept as handles,
        // not views: a destroyed cubemap's handle never matches again, even if Vulkan reuses its view's value.
        Handel<Cubemap> skyboxInSet[max_frames_in_flight]{};
    public:
        Camera camera;
        // Empty, or destroyed: no skybox. Shaders then see the default cubemap at binding 2.
        Handel<Cubemap> skybox;
        // Same convention as a directional light. Intensity 0 means no sun.
        glm::vec3 sunDirection = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
        glm::vec3 sunColor{1.0f};
        float sunIntensity = 0.0f;
        void addInstance(Handel<Instance> instance);
        void removeInstance(Handel<Instance> instance);
        const std::vector<Handel<Instance>>& getInstances() const { return instances; }
        // Does nothing if the light is already in the scene.
        void addLight(Handel<Light> light);
        void removeLight(Handel<Light> light);
        const std::vector<Handel<Light>>& getLights() const { return lights; }
        // Writes this frame's camera + lights and binds the bound pipeline and set 0. Draws nothing.
        Result<void> bindFrameData(VkCommandBuffer commandBuffer);
        // bindFrameData, then every visible instance.
        Result<void> draw(VkCommandBuffer commandBuffer);
        // Draws the skybox behind everything drawn so far, if the scene has one (3D only). Binds the skybox
        // pipeline, so draw it last. Needs bindFrameData() earlier in the same frame.
        Result<void> drawSkybox(VkCommandBuffer commandBuffer);
        Result<void> createUboAndLights();
    };
}
