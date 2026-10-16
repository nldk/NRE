#version 450
layout(location = 0) in vec3 inNormal;
layout(location = 0) out vec4 outColor;

vec3 lightDir = normalize(vec3(0.5, 1, 0.3));
void main() {
    float brightness = max(dot(normalize(inNormal), lightDir), 0.0) * 0.8 + 0.;
    outColor = vec4(brightness,brightness,brightness, 1.0);
}