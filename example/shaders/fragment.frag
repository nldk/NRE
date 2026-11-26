#version 450
// Lit by NRE: the material's textures and factors, the sun, the scene's lights and the sky.
// The output is linear HDR light; the renderer tone maps it at the end of the frame.
#include "nre/lighting.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inWorldPos;
layout(location = 0) out vec4 outColor;

void main() {
    Surface s = nreMaterialSurface(inWorldPos, inNormal, inUV);
    outColor = vec4(nreShade(s), 1.0);
}
