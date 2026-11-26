#version 450
// Unlit: each sun shadow cascade in its debug tint (nreCascadeDebugColor), white beyond the shadow distance.
#include "nre/lighting.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inWorldPos;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(nreCascadeDebugColor(inWorldPos), 1.0);
}
