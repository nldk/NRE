#version 450
layout(push_constant) uniform PC {
    mat4 model;
    mat4 normalMatrix;
} pc;
layout(set = 0, binding = 0) uniform Frame {
    mat4 viewProj;
    vec4 cameraPos;
    uint lightCount;
} frame;
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 0) out vec3 normalOut;
layout(location = 1) out vec2 outUV;
layout(location = 2) out vec3 worldPos;   // the fragment shader needs it for point lights and the view direction

void main() {
    vec4 world = pc.model * vec4(inPosition, 1.0);
    worldPos = world.xyz;
    gl_Position = frame.viewProj * world;
    normalOut = mat3(pc.normalMatrix) * inNormal;
    outUV = inUV;
}