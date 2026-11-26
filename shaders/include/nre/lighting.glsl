// nre/lighting.glsl: NRE's lighting for fragment shaders. Declares set 0 (nre/frame.glsl) and set 1 (the bound
// material), and the functions below. The short form of a lit shader:
//
//     #include "nre/lighting.glsl"
//     void main() {
//         Surface s = nreMaterialSurface(inWorldPos, inNormal, inUV);
//         outColor = vec4(nreShade(s), 1.0);
//     }
//
// Colours are linear. The output is linear HDR light: the renderer tone maps it (with the scene's exposure) at the
// end of the frame, so don't tone map in your shader.
#ifndef NRE_LIGHTING_GLSL
#define NRE_LIGHTING_GLSL

#include "frame.glsl"

// ---- Set 1: the material ---------------------------------------------------------------------------------------
// Unset textures are white (base colour, metallic-roughness) and flat (normal), so the factors alone apply.
layout(set = 1, binding = 0) uniform sampler2D nreBaseColorTexture;          // sRGB, decoded on sampling
layout(set = 1, binding = 1) uniform sampler2D nreMetallicRoughnessTexture;  // glTF layout: g = roughness, b = metallic
layout(set = 1, binding = 2) uniform sampler2D nreNormalTexture;             // tangent space, glTF convention (+Y up)
// Must match GpuMaterial in src/renderer/gpu/UBOAndSSBO.h.
layout(set = 1, binding = 3) uniform NreMaterial {
    vec4  baseColorFactor;
    float metallic;
    float roughness;
    float alphaCutoff;
    float normalScale;
    uint  flags;       // NRE_MATERIAL_ALPHA_MASK | NRE_MATERIAL_DOUBLE_SIDED
} nreMaterial;

const uint NRE_MATERIAL_ALPHA_MASK = 1u;
const uint NRE_MATERIAL_DOUBLE_SIDED = 2u;
const float NRE_PI = 3.14159265;

struct Surface {
    vec3  position;    // world space
    vec3  normal;      // world space, normal map applied, flipped for back faces of double-sided materials
    vec3  geometricNormal;   // world space, the interpolated vertex normal (no normal map), flipped like normal.
                             // The shadow lookup offsets along it: a normal map could tilt it into the surface.
    vec3  view;        // unit vector from the surface toward the camera
    vec3  albedo;      // linear
    float alpha;
    float roughness;   // perceptual, 0..1
    float metallic;
};

// Reads the bound material (textures x factors), applies the normal map, and discards the fragment if the
// material is alpha-masked and alpha < cutoff.
Surface nreMaterialSurface(vec3 worldPos, vec3 vertexNormal, vec2 uv);
// Everything: sun (with shadows) + scene lights + sky ambient and reflections, then haze. Linear HDR out.
vec3 nreShade(Surface s);

// The pieces nreShade is made of, for custom looks:
vec3  nreSunLight(Surface s);                     // direct sun, BRDF x shadow x atmosphere tint
float nreSunShadow(vec3 worldPos, vec3 normal);   // 0 = fully shadowed, 1 = lit (PCF, cascade blending).
                                                  // normal: the geometric one (Surface.geometricNormal), not the
                                                  // normal-mapped one; the lookup is offset along it
vec3  nreSunToLight();                            // unit vector toward the sun
vec3  nreSunRadiance();                           // sun colour x intensity x atmosphere tint
vec3  nreLights(Surface s);                       // point + directional lights, unshadowed
vec3  nreAmbient(Surface s);                      // sky diffuse + specular reflections
vec3  nreBRDF(Surface s, vec3 toLight, vec3 radiance);   // GGX + Lambert, for your own lights
vec3  nreApplyHaze(vec3 color, vec3 worldPos);
vec3  nreCascadeDebugColor(vec3 worldPos);        // tints each cascade, for tuning: red, green, blue, yellow
                                                  // (white beyond the shadow distance). Multiply your colour by it

// ---- Implementation --------------------------------------------------------------------------------------------
// Names starting with nre_ are internal and may change.

// Perceptual roughness is squared for the maths; the floor keeps GGX finite for a perfect mirror.
float nre_alphaRoughness(float roughness) {
    float r = clamp(roughness, 0.045, 1.0);
    return r * r;
}

// D: how many microfacets face the half vector.
float nre_distributionGGX(float NdotH, float a) {
    float a2 = a * a;
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (NRE_PI * d * d);
}

// V: microfacets hidden by their neighbours (height-correlated Smith, as in Filament).
float nre_visibilitySmith(float NdotV, float NdotL, float a) {
    float a2 = a * a;
    float v = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
    float l = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
    return 0.5 / max(v + l, 1e-7);
}

// F: reflection grows toward grazing angles.
vec3 nre_fresnelSchlick(float VdotH, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
}

vec3 nre_F0(Surface s) {
    return mix(vec3(0.04), s.albedo, s.metallic);
}

Surface nreMaterialSurface(vec3 worldPos, vec3 vertexNormal, vec2 uv) {
    // Every texture read and derivative comes before the discard, where all fragments of the quad still run.
    vec4 base = texture(nreBaseColorTexture, uv) * nreMaterial.baseColorFactor;
    vec4 metallicRoughness = texture(nreMetallicRoughnessTexture, uv);
    vec3 mapped = texture(nreNormalTexture, uv).xyz * 2.0 - 1.0;
    mapped.xy *= nreMaterial.normalScale;
    vec3 dpdx = dFdx(worldPos), dpdy = dFdy(worldPos);
    vec2 duvdx = dFdx(uv), duvdy = dFdy(uv);

    Surface s;
    s.position = worldPos;
    s.view = normalize(nreFrame.cameraPos.xyz - worldPos);
    s.albedo = base.rgb;
    s.alpha = base.a;
    s.roughness = clamp(nreMaterial.roughness * metallicRoughness.g, 0.0, 1.0);
    s.metallic = clamp(nreMaterial.metallic * metallicRoughness.b, 0.0, 1.0);

    // The front side's normal. The normal map is applied to it, and back faces of double-sided materials get the
    // whole result flipped at the end (the back of a bump is a dent, so the tilt flips too).
    vec3 n = vertexNormal;
    if (dot(n, n) < 1e-12) {
        // A mesh without normals: the face's own normal, turned toward the camera, then to the front side.
        n = cross(dpdx, dpdy);
        if (dot(n, s.view) < 0.0) n = -n;
        if (dot(n, n) < 1e-24) n = s.view;   // a degenerate triangle: anything finite will do
        if (!gl_FrontFacing) n = -n;
    }
    n = normalize(n);
    vec3 geometric = n;

    // Normal map, in a tangent frame built from the screen-space derivatives of position and UV (no vertex
    // tangents needed). Without usable UVs (none, or degenerate) the UV derivatives' determinant is ~0: the
    // normal is used as it is then.
    float det = duvdx.x * duvdy.y - duvdx.y * duvdy.x;
    if (abs(det) > 1e-14) {
        // Schüler's cotangent frame: T and B point along the gradients of u and v, times
        // dot(n, cross(dpdx, dpdy)). That factor's sign depends on the framebuffer's y direction (negative for
        // camera-facing surfaces in Vulkan) and on which side n is, so it's divided out.
        vec3 dp2perp = cross(dpdy, n);
        vec3 dp1perp = cross(n, dpdx);
        float side = dot(n, cross(dpdx, dpdy)) < 0.0 ? -1.0 : 1.0;
        vec3 T = (dp2perp * duvdx.x + dp1perp * duvdy.x) * side;   // along +u
        vec3 B = (dp2perp * duvdx.y + dp1perp * duvdy.y) * side;   // along +v
        // Each normalized on its own, so a texture stretched more along u than v still tilts by its own angle.
        if (dot(T, T) > 0.0 && dot(B, B) > 0.0) {
            // glTF's +Y points up the image, which is -v (its UV origin is the top left).
            vec3 perturbed = normalize(T) * mapped.x - normalize(B) * mapped.y + n * mapped.z;
            if (dot(perturbed, perturbed) > 1e-12) n = normalize(perturbed);
        }
    }
    bool backFace = (nreMaterial.flags & NRE_MATERIAL_DOUBLE_SIDED) != 0u && !gl_FrontFacing;
    s.normal = backFace ? -n : n;
    s.geometricNormal = backFace ? -geometric : geometric;

    if ((nreMaterial.flags & NRE_MATERIAL_ALPHA_MASK) != 0u && s.alpha < nreMaterial.alphaCutoff) discard;
    return s;
}

vec3 nreBRDF(Surface s, vec3 toLight, vec3 radiance) {
    vec3 N = s.normal;
    float NdotL = dot(N, toLight);
    if (NdotL <= 0.0) return vec3(0.0);   // the light is behind the surface
    float a = nre_alphaRoughness(s.roughness);
    float NdotV = max(dot(N, s.view), 1e-4);
    vec3 h = s.view + toLight;
    vec3 H = dot(h, h) > 1e-12 ? normalize(h) : N;   // half vector between eye and light
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(s.view, H), 0.0);

    vec3 F = nre_fresnelSchlick(VdotH, nre_F0(s));
    vec3 specular = nre_distributionGGX(NdotH, a) * nre_visibilitySmith(NdotV, NdotL, a) * F;
    vec3 diffuse = (1.0 - F) * (1.0 - s.metallic) * s.albedo / NRE_PI;   // what isn't reflected scatters; metals don't
    return (diffuse + specular) * radiance * NdotL;
}

vec3 nreSunToLight() {
    return -nreFrame.sunDirection.xyz;
}

vec3 nreSunRadiance() {
    return nreFrame.sunColor.rgb * nreFrame.sunColor.w;
}

// Where the blend from cascade i into cascade i + 1 starts: the last 10% of cascade i. Must match
// LightingMath::cascadeBlendStart (the next cascade's slice starts there).
float nre_cascadeBlendStart(uint i) {
    float begin = i > 0u ? nreFrame.cascadeSplits[i - 1u] : 0.0;
    return nreFrame.cascadeSplits[i] - 0.1 * (nreFrame.cascadeSplits[i] - begin);
}

// The first cascade whose range reaches viewDepth (the caller checks it's inside the shadow distance).
uint nre_cascadeAt(float viewDepth, uint count) {
    uint cascade = 0u;
    while (cascade + 1u < count && viewDepth >= nreFrame.cascadeSplits[cascade]) ++cascade;
    return cascade;
}

// Distance along the camera's view direction: the clip-space w of its perspective projection.
float nre_viewDepth(vec3 worldPos) {
    return (nreFrame.viewProj * vec4(worldPos, 1.0)).w;
}

// 0..1 lit, from one cascade's map, with 3x3 PCF.
float nre_cascadeShadow(vec3 worldPos, vec3 normal, uint cascade) {
    mat4 lightViewProj = nreFrame.cascadeViewProj[cascade];

    // Normal offset: look up a point slightly off the surface, so it doesn't shadow itself (acne). Measured in
    // texels of this cascade: the orthographic projection scales light-space x by 2 / (the cascade's width).
    vec2 mapSize = vec2(textureSize(nreShadowMap, 0).xy);
    float texelWorld = 2.0 / (length(vec3(lightViewProj[0][0], lightViewProj[1][0], lightViewProj[2][0])) * mapSize.x);
    vec3 p = worldPos + normal * (nreFrame.shadowParams.y * texelWorld);

    // Orthographic (w = 1), standard depth (0 at the light) and no Y flip: uv = ndc.xy * 0.5 + 0.5.
    vec3 ndc = (lightViewProj * vec4(p, 1.0)).xyz;
    if (ndc.z >= 1.0) return 1.0;   // behind the cascade's far end
    vec2 uv = ndc.xy * 0.5 + 0.5;
    float depth = max(ndc.z, 0.0);  // sunward of the near plane: casters there were clamped to 0 too

    // 3x3 PCF; each tap is itself a bilinear 2x2 comparison. Outside the map the border is lit. Zero gradients:
    // the map has one mip level, and this runs in non-uniform control flow, where implicit derivatives aren't defined.
    vec2 texel = 1.0 / mapSize;
    float lit = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            lit += textureGrad(nreShadowMap, vec4(uv + vec2(x, y) * texel, float(cascade), depth), vec2(0.0), vec2(0.0));
        }
    }
    return lit / 9.0;
}

float nreSunShadow(vec3 worldPos, vec3 normal) {
    uint count = uint(nreFrame.shadowParams.x);
    if (count == 0u) return 1.0;
    float viewDepth = nre_viewDepth(worldPos);
    float shadowEnd = nreFrame.cascadeSplits[count - 1u];
    if (viewDepth >= shadowEnd) return 1.0;
    normal = normalize(normal);

    uint cascade = nre_cascadeAt(viewDepth, count);
    float lit = nre_cascadeShadow(worldPos, normal, cascade);
    if (cascade + 1u < count) {
        // Over the last 10% of a cascade, blend into the next one so the switch doesn't show as a line. Only
        // there: everywhere else it's one lookup.
        float blendStart = nre_cascadeBlendStart(cascade);
        if (viewDepth > blendStart) {
            float t = (viewDepth - blendStart) / (nreFrame.cascadeSplits[cascade] - blendStart);
            lit = mix(lit, nre_cascadeShadow(worldPos, normal, cascade + 1u), t);
        }
    }
    // The last cascade fades out over the last 10% of the shadow distance instead of ending at a line.
    return mix(lit, 1.0, smoothstep(0.9 * shadowEnd, shadowEnd, viewDepth));
}

vec3 nreSunLight(Surface s) {
    return nreBRDF(s, nreSunToLight(), nreSunRadiance()) * nreSunShadow(s.position, s.geometricNormal);
}

vec3 nreLights(Surface s) {
    vec3 color = vec3(0.0);
    for (uint i = 0u; i < nreFrame.lightCount; ++i) {
        NreLight light = nreLightBuffer.lights[i];
        vec3 toLight;
        float attenuation = 1.0;
        if (light.position.w == 0.0) {
            toLight = -light.position.xyz;
        } else {
            vec3 d = light.position.xyz - s.position;
            float dist2 = max(dot(d, d), 1e-4);
            toLight = d * inversesqrt(dist2);
            attenuation = 1.0 / dist2;   // inverse-square falloff, like real light
        }
        color += nreBRDF(s, toLight, light.color.rgb * light.color.w * attenuation);
    }
    return color;
}

// The sky as a sky/ground hemisphere: radiance from above is the sky colour, from below a dim grey ground.
// blurWidth 1 gives the cosine-weighted average around dir (the irradiance), smaller values sharper reflections.
vec3 nre_hemisphere(vec3 dir, float blurWidth) {
    vec3 sky = nreFrame.skyColor.rgb;
    vec3 ground = vec3(dot(sky, vec3(0.2126, 0.7152, 0.0722)) * 0.3);
    return mix(ground, sky, clamp(0.5 + 0.5 * dir.y / blurWidth, 0.0, 1.0));
}

// Split-sum environment BRDF (scale, bias for F0), Karis' analytic fit for mobile.
vec2 nre_environmentBRDF(float NdotV, float roughness) {
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NdotV)) * r.x + r.y;
    return vec2(-1.04, 1.04) * a004 + r.zw;
}

vec3 nreAmbient(Surface s) {
    vec3 N = s.normal;
    float NdotV = max(dot(N, s.view), 1e-4);
    vec3 diffuse = s.albedo * (1.0 - s.metallic) * nre_hemisphere(N, 1.0);
    vec2 env = nre_environmentBRDF(NdotV, clamp(s.roughness, 0.0, 1.0));
    vec3 R = reflect(-s.view, N);
    vec3 specular = nre_hemisphere(R, max(nre_alphaRoughness(s.roughness), 0.02)) * (nre_F0(s) * env.x + env.y);
    return diffuse + specular;
}

vec3 nreApplyHaze(vec3 color, vec3 worldPos) {
    return color;
}

vec3 nreCascadeDebugColor(vec3 worldPos) {
    // The cascade nreSunShadow starts from (blend bands aren't shown).
    uint count = uint(nreFrame.shadowParams.x);
    float viewDepth = nre_viewDepth(worldPos);
    if (count == 0u || viewDepth >= nreFrame.cascadeSplits[count - 1u]) return vec3(1.0);
    const vec3 tints[4] = vec3[4](vec3(1.0, 0.25, 0.25), vec3(0.25, 1.0, 0.25), vec3(0.25, 0.25, 1.0), vec3(1.0, 1.0, 0.25));
    return tints[nre_cascadeAt(viewDepth, count)];
}

vec3 nreShade(Surface s) {
    return nreApplyHaze(nreSunLight(s) + nreLights(s) + nreAmbient(s), s.position);
}

#endif
