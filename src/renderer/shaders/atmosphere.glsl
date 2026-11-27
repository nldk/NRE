// Built into librenderer: the atmosphere for the sky shaders (transmittanceLut.comp, skyCubemap.comp, sky.frag).
// The constants come from lighting/atmosphere.inc, the same file LightingMath::Atmosphere is made from, as ATM_<name>.
// Planet-centred, y up, kilometres. Heights near the ground are tiny next to the planet's radius, so the maths works
// with q = r^2 - R^2 (R the planet radius) where r^2 - R^2 would otherwise cancel in float.
#ifndef NRE_ATMOSPHERE_GLSL
#define NRE_ATMOSPHERE_GLSL

#define NRE_ATMOSPHERE_SCALAR(name, value) const float ATM_##name = value;
#define NRE_ATMOSPHERE_RGB(name, r, g, b) const vec3 ATM_##name = vec3(r, g, b);
#define NRE_ATMOSPHERE_INT(name, value) const int ATM_##name = value;
#include "lighting/atmosphere.inc"
#undef NRE_ATMOSPHERE_SCALAR
#undef NRE_ATMOSPHERE_RGB
#undef NRE_ATMOSPHERE_INT

const float ATM_PI = 3.14159265;
// Distance from the planet's surface to the top of the atmosphere along a horizontal ray at the ground.
const float ATM_H = sqrt(ATM_topRadius * ATM_topRadius - ATM_planetRadius * ATM_planetRadius);
// The viewer: radius and its q.
const float ATM_viewerRadius = ATM_planetRadius + ATM_viewerHeight;
const float ATM_viewerQ = ATM_viewerHeight * (2.0 * ATM_planetRadius + ATM_viewerHeight);

// Height above the ground for a point with this q (= r^2 - R^2), never negative.
float atm_height(float q) {
    return max(q / (sqrt(q + ATM_planetRadius * ATM_planetRadius) + ATM_planetRadius), 0.0);
}

// Rayleigh, Mie and ozone densities at height h (relative to the ground; ozone: a tent around its layer).
vec3 atm_densities(float h) {
    return vec3(exp(-h / ATM_rayleighScaleHeight), exp(-h / ATM_mieScaleHeight),
                max(0.0, 1.0 - abs(h - ATM_ozoneCenter) / ATM_ozoneHalfWidth));
}

// Extinction per km at height h, per channel.
vec3 atm_extinction(vec3 densities) {
    return densities.x * ATM_rayleighScatter + densities.y * (ATM_mieScatter + ATM_mieAbsorb) + densities.z * ATM_ozoneAbsorb;
}

// The transmittance LUT's parameterisation (Bruneton): x = distance to the top along mu, from straight up (0) to
// the horizon (1); y = sqrt(q) / H, from the ground (0) to the top (1). Only for rays that don't meet the planet.
vec2 atm_lutCoords(float q, float r, float mu) {
    float rho = sqrt(max(q, 0.0));
    float disc = r * r * mu * mu + (ATM_H * ATM_H - q);   // r^2 (mu^2 - 1) + top^2
    float d = max(-r * mu + sqrt(max(disc, 0.0)), 0.0);
    float dMin = ATM_topRadius - r, dMax = rho + ATM_H;
    return vec2(clamp((d - dMin) / max(dMax - dMin, 1e-6), 0.0, 1.0), clamp(rho / ATM_H, 0.0, 1.0));
}

// Transmittance from a point (q, r) toward the top of the atmosphere along mu (cosine from straight up). 0 when the
// ray meets the planet first.
vec3 atm_transmittance(sampler2D lut, float q, float r, float mu) {
    if (mu < 0.0 && q <= r * r * mu * mu) return vec3(0.0);   // the ray meets the planet
    vec2 x = atm_lutCoords(q, r, mu);
    vec2 size = vec2(textureSize(lut, 0));
    return textureLod(lut, (x * (size - 1.0) + 0.5) / size, 0.0).rgb;   // texel centres at x = 0 and 1
}

// Whether a ray from the viewer in direction d meets the ground (it starts 1.5 m up, so slightly below horizontal).
bool atm_viewHitsGround(vec3 d) {
    float b = ATM_viewerRadius * d.y;
    return d.y < 0.0 && b * b >= ATM_viewerQ;
}

#endif
