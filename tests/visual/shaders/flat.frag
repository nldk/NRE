#version 450
layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inWorldPos;
layout(location = 0) out vec4 outColor;

// Unlit magenta: shows geometry without any lighting.
void main() {
    outColor = vec4(1.0, 0.0, 1.0, 1.0);
}
