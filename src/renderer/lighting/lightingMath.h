#pragma once

// CPU-side lighting math: header-only, glm only, so it can be unit-tested without Vulkan.

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

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

// ---- Cascaded sun shadows -------------------------------------------------------------------------------------

// Cascades for a shadow distance in metres: <= 30 -> 1, <= 100 -> 2, <= 300 -> 3, else 4; 0 -> 0.
inline uint32_t cascadeCountFor(float shadowDistance) {
    if (shadowDistance <= 0.0f) return 0;
    if (shadowDistance <= 30.0f) return 1;
    if (shadowDistance <= 100.0f) return 2;
    if (shadowDistance <= 300.0f) return 3;
    return 4;
}

// World-space corners of the camera frustum between two view distances: 4 near corners, then 4 far corners
// (each ordered -x-y, +x-y, +x+y, -x+y in view space). `view` is world -> view, looking down -z.
inline std::array<glm::vec3, 8> frustumSliceCorners(const glm::mat4& view, float fovY, float aspect, float sliceNear, float sliceFar) {
    glm::mat4 inv = glm::inverse(view);
    float tanHalf = std::tan(fovY * 0.5f);
    constexpr float sx[4] = {-1, 1, 1, -1};
    constexpr float sy[4] = {-1, -1, 1, 1};
    std::array<glm::vec3, 8> out{};
    for (int i = 0; i < 8; ++i) {
        float d = i < 4 ? sliceNear : sliceFar;
        float hy = tanHalf * d;
        float hx = hy * aspect;
        out[i] = glm::vec3(inv * glm::vec4(sx[i & 3] * hx, sy[i & 3] * hy, -d, 1.0f));
    }
    return out;
}

// Light view-projection for one cascade. `sunDirection` is the direction the light travels (toward the ground).
// Bounding-sphere fit (rotation-stable size), standard-Z orthographic depth (0 near the light, 1 far; glm is built
// with GLM_FORCE_DEPTH_ZERO_TO_ONE), centre snapped to whole texels in light space so the map doesn't swim.
// Contract for the shadow pass and shader:
//  - The depth range is tight around the slice's bounding sphere (best precision), so casters sunward of it land in
//    front of the near plane (clip z < 0). The shadow pipeline MUST enable depth clamping (depthClampEnable =
//    VK_TRUE, device feature `depthClamp`) so they clamp to depth 0 ("pancaking") and still cast.
//  - Standard depth (0 at the light) and NO Y flip, unlike the main camera pass: shadow lookups use
//    uv = ndc.xy * 0.5 + 0.5. Because the main pass flips Y, triangle winding is mirrored relative to it: use cull
//    mode NONE (as planned) or flip frontFace.
inline glm::mat4 cascadeViewProj(const std::array<glm::vec3, 8>& corners, glm::vec3 sunDirection, uint32_t resolution) {
    glm::vec3 dir = glm::normalize(sunDirection);
    glm::vec3 up = std::fabs(dir.y) > 0.99f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);

    glm::vec3 centre(0.0f);
    for (const glm::vec3& c : corners) centre += c;
    centre /= 8.0f;
    float radius = 0.0f;
    for (const glm::vec3& c : corners) radius = std::max(radius, glm::length(c - centre));
    radius = std::ceil(radius * 16.0f) / 16.0f; // quantised so tiny float noise can't change the texel size

    // Snap in a light frame that doesn't depend on the centre.
    glm::mat4 lightRot = glm::lookAt(glm::vec3(0.0f), dir, up);
    glm::vec3 c = glm::vec3(lightRot * glm::vec4(centre, 1.0f));
    float texel = 2.0f * radius / float(resolution);
    c.x = std::floor(c.x / texel) * texel;
    c.y = std::floor(c.y / texel) * texel;
    centre = glm::vec3(glm::inverse(lightRot) * glm::vec4(c, 1.0f));

    float pullBack = 2.0f * radius; // room for casters between the sun and the slice
    glm::mat4 view = glm::lookAt(centre - dir * pullBack, centre, up);
    glm::mat4 proj = glm::ortho(-radius, radius, -radius, radius, 0.0f, pullBack + radius);
    return proj * view;
}

// Practical split scheme (log/uniform blend). Entry i is the view distance where cascade i ends; the last used
// entry and all unused ones equal shadowDistance.
inline std::array<float, 4> cascadeSplits(float nearPlane, float shadowDistance, uint32_t count, float lambda = 0.75f) {
    std::array<float, 4> s;
    s.fill(shadowDistance);
    count = std::min(count, 4u);
    for (uint32_t i = 0; i + 1 < count; ++i) {
        float p = float(i + 1) / float(count);
        float logSplit = nearPlane * std::pow(shadowDistance / nearPlane, p);
        float linSplit = nearPlane + (shadowDistance - nearPlane) * p;
        s[i] = lambda * logSplit + (1.0f - lambda) * linSplit;
    }
    return s;
}

// ---- Atmosphere ------------------------------------------------------------------------------------------------
// Settled with tests/tools/skyPrototype.py (see docs/superpowers/notes/2026-11-26-sky-prototype.md). The Task 8
// shaders (transmittanceLut.comp and the sky) copy these values. Lengths are kilometres, coefficients 1/km.
namespace Atmosphere {
inline constexpr double planetRadius = 6360.0;
inline constexpr double topRadius = 6460.0;
inline constexpr double viewerHeight = 0.0015; // 1.5 m
inline constexpr double rayleighScatter[3] = {5.802e-3, 13.558e-3, 33.1e-3};
inline constexpr double rayleighScaleHeight = 8.0;
inline constexpr double mieScatter = 3.996e-3;
inline constexpr double mieAbsorb = 4.40e-3;
inline constexpr double mieScaleHeight = 1.2;
inline constexpr double mieG = 0.8;
inline constexpr double ozoneAbsorb[3] = {0.650e-3, 1.881e-3, 0.085e-3};
inline constexpr double ozoneCenter = 25.0;    // tent profile: 1 at the centre, 0 at +-ozoneHalfWidth
inline constexpr double ozoneHalfWidth = 15.0;
inline constexpr int transmittanceSteps = 64;  // midpoint rule with quadratic spacing t = tMax * u^2
inline constexpr double skyScale = 10.0;       // single-scatter radiance gain (stands in for multiple scattering)
inline constexpr double sunAngularRadius = 0.0046690; // 0.2675 deg in radians
// The sun's light fades out as the disk sinks: factor 0 at sunFadeLowDeg and below, 1 at sunFadeHighDeg and above
// (smoothstep). Wide enough (about two disk diameters) that a running sunset never pops; full strength at the horizon.
inline constexpr double sunFadeLowDeg = -0.95;
inline constexpr double sunFadeHighDeg = 0.0;
inline constexpr double nightFloor[3] = {0.0025, 0.0045, 0.0100}; // linear sky radiance at sun intensity 3
// Auto exposure: 1 at >= exposureHighDeg, exp(ln(exposureMax) * smoothstep) down to exposureLowDeg and below.
inline constexpr double exposureMax = 5.0;
inline constexpr double exposureHighDeg = 30.0;
inline constexpr double exposureLowDeg = -10.0;
} // namespace Atmosphere

// RGB transmittance of the sun's light reaching a viewer 1.5 m above an Earth-sized planet (Rayleigh + Mie +
// ozone). `sunElevation` is radians above the horizon. The optical depth is evaluated at max(elevation, 0) (never
// NaN/inf) and multiplied by the single smooth horizon fade below, so the result reaches exactly 0 from -0.95 degrees down.
inline glm::vec3 sunTransmittance(float sunElevation) {
    namespace A = Atmosphere;
    const double py = A::planetRadius + A::viewerHeight;
    const double elevDeg = double(sunElevation) * 180.0 / 3.14159265358979323846;
    double fade = std::clamp((elevDeg - A::sunFadeLowDeg) / (A::sunFadeHighDeg - A::sunFadeLowDeg), 0.0, 1.0);
    fade = fade * fade * (3.0 - 2.0 * fade);
    if (fade <= 0.0) return glm::vec3(0.0f);
    const double dy = std::sin(std::max(double(sunElevation), 0.0));
    const double b = py * dy;
    const double c = py * py;
    const double planetDisc = b * b - (c - A::planetRadius * A::planetRadius);
    (void)planetDisc; // dy >= 0, so the sun ray never meets the planet
    const double tTop = -b + std::sqrt(b * b - (c - A::topRadius * A::topRadius));
    const double horizontal = std::sqrt(std::max(1.0 - dy * dy, 0.0));
    double depth[3] = {0, 0, 0};
    for (int i = 0; i < A::transmittanceSteps; ++i) {
        double u = (i + 0.5) / A::transmittanceSteps;
        double t = tTop * u * u;
        double dt = tTop * 2.0 * u / A::transmittanceSteps;
        double px = t * horizontal;
        double h = std::max(std::sqrt(px * px + (py + t * dy) * (py + t * dy)) - A::planetRadius, 0.0);
        double r = std::exp(-h / A::rayleighScaleHeight);
        double m = std::exp(-h / A::mieScaleHeight);
        double o = std::max(0.0, 1.0 - std::fabs(h - A::ozoneCenter) / A::ozoneHalfWidth);
        for (int k = 0; k < 3; ++k)
            depth[k] += (r * A::rayleighScatter[k] + m * (A::mieScatter + A::mieAbsorb) + o * A::ozoneAbsorb[k]) * dt;
    }
    return {float(fade * std::exp(-depth[0])), float(fade * std::exp(-depth[1])), float(fade * std::exp(-depth[2]))};
}

// Brightness compensation driven by the sun's elevation (radians): 1 from 30 degrees up, rising smoothly toward
// the horizon and below, capped at the night level from -10 degrees down. Dusk stays darker than noon because the
// rise is much smaller than the actual drop in light.
inline float autoExposure(float sunElevation) {
    namespace A = Atmosphere;
    double deg = double(sunElevation) * 180.0 / 3.14159265358979323846;
    double t = std::clamp((deg - A::exposureHighDeg) / (A::exposureLowDeg - A::exposureHighDeg), 0.0, 1.0);
    t = t * t * (3.0 - 2.0 * t);
    return float(std::exp(std::log(A::exposureMax) * t));
}

// ---- Haze ------------------------------------------------------------------------------------------------------

// Extinction per metre that leaves 5% of the light at `visibility` metres (ln 20 / visibility); 0 turns haze off.
inline float hazeDensityForVisibility(float visibility) {
    return visibility > 0.0f ? std::log(20.0f) / visibility : 0.0f;
}

} // namespace NRE::LightingMath
