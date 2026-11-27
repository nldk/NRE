// Built into librenderer: GGX importance sampling for the IBL passes (brdfLut.comp, prefilter.comp). The BRDF terms
// match nre/lighting.glsl (nre_alphaRoughness, nre_distributionGGX, nre_visibilitySmith), so the image-based light
// agrees with the direct light.
#ifndef NRE_GGX_GLSL
#define NRE_GGX_GLSL

const float GGX_PI = 3.14159265;

// Perceptual roughness -> GGX alpha, with nre_alphaRoughness's floor.
float ggxAlpha(float roughness) {
    float r = clamp(roughness, 0.045, 1.0);
    return r * r;
}

float ggxDistribution(float NdotH, float a) {
    float a2 = a * a;
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (GGX_PI * d * d);
}

// Height-correlated Smith visibility (as nre_visibilitySmith).
float ggxVisibility(float NdotV, float NdotL, float a) {
    float a2 = a * a;
    float v = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
    float l = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
    return 0.5 / max(v + l, 1e-7);
}

// The i-th of n points of the Hammersley set in [0, 1)^2.
vec2 hammersley(uint i, uint n) {
    return vec2((float(i) + 0.5) / float(n), float(bitfieldReverse(i)) * 2.3283064365386963e-10);
}

// A half vector around +z, distributed as D(h) (h.z): pdf(h) = D * h.z.
vec3 sampleGGX(vec2 xi, float a) {
    float phi = 2.0 * GGX_PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));
    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

// Columns: tangent, bitangent, n. Turns a vector around +z into one around n.
mat3 tangentFrame(vec3 n) {
    vec3 t = normalize(cross(abs(n.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0), n));
    return mat3(t, cross(n, t), n);
}

#endif
