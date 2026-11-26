#version 450
// Built into librenderer (lighting/shadows.cpp): draws a mesh into one cascade of the sun's shadow map.
// Standard depth (0 at the light) and no Y flip; see LightingMath::cascadeViewProj.
#include "nre/frame.glsl"

layout(push_constant) uniform PC {
    mat4 model;      // per draw (Model::drawDepthOnly)
    uint cascade;    // per cascade (ShadowMaps::record): which nreFrame.cascadeViewProj to use
} pc;
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 0) out vec2 outUV;   // for shadowAlpha.frag; unused by the depth-only pipeline

void main() {
    gl_Position = nreFrame.cascadeViewProj[pc.cascade] * (pc.model * vec4(inPosition, 1.0));
    outUV = inUV;
}
