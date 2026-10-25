#pragma once
#include <vector>
#include <glm/glm.hpp>

#include "renderer.h"
#include "gpu/UBOAndSSBO.h"

namespace NRE {
    // A light the user creates and adds to scenes. Scene::draw packs every light of the scene into a GpuLight each frame.
    struct Light {
        bool directional = true;
        glm::vec3 position{0.0f};                  // point lights
        glm::vec3 direction{0.0f, -1.0f, 0.0f};    // directional lights: the way the light travels, normalized
        glm::vec3 color{1.0f};
        float intensity = 1.0f;
        std::vector<Handel<Scene>> scenes;         // so destroying it can remove it from them

        // Layout the shader expects: position.w = 0 directional (xyz = direction), 1 point (xyz = position).
        GpuLight toGpu() const {
            return {directional ? glm::vec4(direction, 0.0f) : glm::vec4(position, 1.0f), glm::vec4(color, intensity)};
        }
    };
}
