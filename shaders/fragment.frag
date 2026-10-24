#version 450
layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV;
layout(location = 0) out vec4 outColor;
layout(set = 0, binding = 0) uniform sampler2D baseTexture;

vec3 lightDir = normalize(vec3(0.5, 1, 0.3));
void main() {
    float brightness = max(dot(normalize(inNormal), lightDir), 0.0) * 0.8 + 0.2;
    outColor = vec4(texture(baseTexture,inUV).rgb * brightness, 1.0);
}