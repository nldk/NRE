// nre/frame.glsl: NRE's per-frame data, descriptor set 0. Enough for vertex shaders; nre/lighting.glsl includes
// it for fragment shaders. nre_compile_shaders() puts NRE's shader include folder on the include path, so
//     #include "nre/frame.glsl"
// works in any shader it compiles. The push constants (mat4 model, mat4 normalMatrix) stay in your vertex shader.
#ifndef NRE_FRAME_GLSL
#define NRE_FRAME_GLSL

// Must match GpuLight in src/renderer/gpu/UBOAndSSBO.h.
struct NreLight {
    vec4 position;   // w = 0: directional, xyz = the way the light travels. w = 1: point light, xyz = position
    vec4 color;      // rgb = colour, w = intensity
};

// Must match FrameData in src/renderer/gpu/UBOAndSSBO.h (std140; the offsets are in the comments).
layout(set = 0, binding = 0) uniform NreFrame {
    mat4 viewProj;                 // 0: world -> clip (reverse-Z, infinite far plane)
    vec4 cameraPos;                // 64: xyz world position
    uint lightCount;               // 80: entries in nreLightBuffer
    vec4 sunDirection;             // 96: xyz: the way the sunlight travels (normalized)
    vec4 sunColor;                 // 112: rgb: colour, w: intensity (0 = no sun)
    mat4 cascadeViewProj[4];       // 128: world -> shadow clip, per cascade
    vec4 cascadeSplits;            // 384: view-space distance where each cascade ends
    vec4 shadowParams;             // 400: x: cascade count (0 = no sun shadows), y: normal offset in shadow-map texels
    vec4 sunRadiance;              // 416: rgb: sunlight reaching objects (Landscape: through the atmosphere), w: 1 if the sun is up
    vec4 skyColor;                 // 432: rgb: the sky colour as linear light
    vec4 skyParams;                // 448: x: sky source (0 colour, 1 cubemap, 2 procedural), y: specular mips,
                                   //      z: procedural sky's scattering brightness (sun intensity / 3; nreSky
                                   //      already includes it, plus the night floor that doesn't scale), else 0
    vec4 hazeParams;               // 464: x: density per metre (0 = off), y: 1 / haze height
} nreFrame;

// The scene's point and directional lights: nreFrame.lightCount of them.
layout(set = 0, binding = 1) readonly buffer NreLightBuffer { NreLight lights[]; } nreLightBuffer;

// The sky (nreFrame.skyParams.x): the scene's skybox cubemap (display values), the procedural sky (linear light as
// drawn, sun disk excepted, with mips), or a black one.
layout(set = 0, binding = 2) uniform samplerCube nreSky;

// The sun's shadow maps: one layer per cascade (nreFrame.shadowParams.x of them), standard depth (0 at the light).
// A comparison sampler: texture(nreShadowMap, vec4(uv, cascade, depth)) gives 1 where lit. Use nreSunShadow.
layout(set = 0, binding = 3) uniform sampler2DArrayShadow nreShadowMap;

#endif
