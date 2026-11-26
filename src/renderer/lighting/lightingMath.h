#pragma once

// CPU-side lighting math: header-only, glm only, so it can be unit-tested without Vulkan.

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace NRE::LightingMath {

// Narkowicz ACES fit. Must match toneMapACES in the shaders.
inline float toneMapACES(float x) {
    return std::clamp((x * (2.51f * x + 0.03f)) / (x * (2.43f * x + 0.59f) + 0.14f), 0.0f, 1.0f);
}

inline glm::vec3 toneMapACES(glm::vec3 c) {
    return {toneMapACES(c.x), toneMapACES(c.y), toneMapACES(c.z)};
}

// Inverse of the fit: solves (2.43y - 2.51)x^2 + (0.59y - 0.03)x + 0.14y = 0 for the positive root.
// y is clamped below 1 because the fit only reaches 1 at infinity.
inline float inverseToneMapACES(float y) {
    y = std::clamp(y, 0.0f, 0.999f);
    float a = 2.43f * y - 2.51f; // always negative on [0, 0.999]
    float b = 0.59f * y - 0.03f;
    float c = 0.14f * y;
    float disc = std::max(b * b - 4.0f * a * c, 0.0f);
    return (-b - std::sqrt(disc)) / (2.0f * a);
}

inline glm::vec3 inverseToneMapACES(glm::vec3 c) {
    return {inverseToneMapACES(c.x), inverseToneMapACES(c.y), inverseToneMapACES(c.z)};
}

// Reverse-Z with an infinite far plane: depth is 1 at nearPlane and tends to 0 at infinity,
// which spreads float precision evenly over distance. Y is flipped for Vulkan.
inline glm::mat4 reverseZPerspective(float fovY, float aspect, float nearPlane) {
    float f = 1.0f / std::tan(fovY * 0.5f);
    glm::mat4 m(0.0f);
    m[0][0] = f / aspect;
    m[1][1] = -f;
    m[2][3] = -1.0f;      // w = -z_view
    m[3][2] = nearPlane;  // z_clip = near, so depth = near / distance
    return m;
}

// 2D projection, origin bottom-left like before, depth reversed: +z (toward the viewer) gets the larger depth.
inline glm::mat4 reverseZOrtho(float width, float height, float depthRange) {
    glm::mat4 m = glm::ortho(0.0f, width, 0.0f, height, -depthRange, depthRange);
    m[2][2] = 1.0f / (2.0f * depthRange);
    m[3][2] = 0.5f;
    return m;
}

} // namespace NRE::LightingMath
