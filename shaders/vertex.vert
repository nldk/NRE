#version 450
layout(push_constant) uniform PC {
    mat4 model;
    mat4 viewProj;
} pc;
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 0) out vec3 normalOut;

void main() {
    gl_Position = pc.viewProj * pc.model * vec4(inPosition, 1.0);
    normalOut = (pc.model * vec4(inNormal,0)).xyz;
}