#pragma once
#include <cmath>
#include <vector>

#include "gpu/buffer.h"
#include "renderer.h"
#include "gpu/UBOAndSSBO.h"
#include "core/frameRecorder.h"

namespace NRE {

    struct Camera {
        static constexpr glm::vec3 worldUp{0.0f, 1.0f, 0.0f};
        static constexpr float maxPitch = 1.5533430f;
        // The renderer's perspective (Render_3D): vertical field of view (60 degrees) and near plane, in metres.
        static constexpr float fovY = 1.0471976f;
        static constexpr float nearPlane = 0.1f;

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
    // Where the scene's sky comes from this frame (FrameData::skyParams.x).
    enum class SkySource : uint32_t {
        Color = 0,        // the sky colour: the clear colour shows, and the ambient is a hemisphere made from it
        Cubemap = 1,      // the skybox (LDR, shown as authored)
        Procedural = 2,   // the atmosphere from the sun (Sky)
    };

    class Scene {
        std::vector<Handel<Instance>> instances;
        std::vector<Handel<Light>> lights;
        std::vector<GpuLight> gpuLights;   // rebuilt from lights every frame; a member so it keeps its capacity
        VkDescriptorSet frameSets[max_frames_in_flight]{};
        PerFrameBuffer UBO;
        PerFrameBuffer Lights;
        // What bindings 2 (the sky), 4 and 5 (its IBL) of each frame's set point at: the skybox and its IBL, the
        // procedural sky (that frame slot's cubemap and IBL), or with neither the default (black) cubemap. Kept as
        // handles, not views: a destroyed cubemap's handle never matches again, even if Vulkan reuses its view's value.
        struct SkyBinding {
            Handel<Cubemap> skybox;
            bool procedural = false;
            bool ibl = false;   // 4 and 5 show the sky's IBL (else the default cubemap)
            bool operator==(const SkyBinding&) const = default;
        };
        SkyBinding skyInSet[max_frames_in_flight]{};
    public:
        Camera camera;
        // Empty, or destroyed: no skybox. Landscape then shows the procedural sky at binding 2 (or, with proceduralSky
        // off, the default cubemap).
        Handel<Cubemap> skybox;
        // Same convention as a directional light. Intensity 0 means no sun.
        glm::vec3 sunDirection = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
        glm::vec3 sunColor{1.0f};
        float sunIntensity = 0.0f;
        static constexpr glm::vec3 defaultSkyColor{0.1f, 0.3f, 0.6f};
        // setExposure: multiplies the scene's light before tone mapping. Always > 0.
        float exposure = 1.0f;
        // setSkyColor: the background where nothing is drawn, as a display colour (looks the same at any exposure).
        glm::vec3 skyColor = defaultSkyColor;
        // setSceneLighting: picks the defaults applyLightingDefaults() resets the settings below to (createScene()
        // applies them too). Landscape also tints the sun by the atmosphere and adds the automatic exposure.
        SceneLighting lighting = SceneLighting::Small;
        // Set by applyLightingDefaults() only (it's the one place the per-mode defaults live):
        // setShadowDistance: how far from the camera the sun casts shadows, in metres. 0 = no sun shadows. Never negative.
        float shadowDistance{};
        // The sky without a skybox: the procedural atmosphere (Landscape) or the sky colour (Small).
        bool proceduralSky{};
        // setHaze: the distance at which the haze hides about 95% (metres, 0 = no haze) and how thick its layer is
        // (metres above y = 0, > 0).
        float hazeVisibility{};
        float hazeHeight = 300.0f;
        // Sun shadow cascades this frame (0 = none), set by writeFrameData() with their matrices; the shadow pass draws
        // this many.
        uint32_t shadowCascades = 0;
        // This frame's sky source and tone-map exposure (setExposure x, in Landscape with the procedural sky, the
        // automatic exposure), set by writeFrameData().
        SkySource skySource = SkySource::Color;
        float frameExposure = 1.0f;
        // The procedural sky's scattering brightness this frame (sun intensity / 3; 0 without a sun or procedural
        // sky), set by writeFrameData() (FrameData::skyParams.z).
        float skyBrightness = 0.0f;
        void addInstance(Handel<Instance> instance);
        void removeInstance(Handel<Instance> instance);
        const std::vector<Handel<Instance>>& getInstances() const { return instances; }
        // Does nothing if the light is already in the scene.
        void addLight(Handel<Light> light);
        void removeLight(Handel<Light> light);
        const std::vector<Handel<Light>>& getLights() const { return lights; }
        // Writes this frame's camera, lights, sun (radiance, shadow cascades), sky source and exposure (and points
        // set 0 at the sky). Only once this frame's fence has been waited on: the GPU may still be reading the previous
        // contents until then.
        Result<void> writeFrameData();
        // Binds pipeline and this frame's set 0 (written by writeFrameData()).
        void bindFrameData(VkCommandBuffer commandBuffer, const Pipeline& pipeline);
        // A draw for every visible instance.
        void appendDraws(std::vector<QueuedDraw>& out) const;
        Result<void> createUboAndLights();
    };

    // Resets the scene's per-mode settings to the defaults of scene.lighting (setSceneLighting), and is how a new
    // scene gets them: Small: shadows 20 m, no haze, the sky colour without a skybox; Landscape: shadows 500 m, haze
    // visibility 8000 m (300 m thick), the procedural sky without a skybox. The sun, lights, exposure, sky colour and
    // skybox are kept.
    void applyLightingDefaults(Scene& scene);
}
