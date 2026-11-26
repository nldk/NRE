#version 450
// Built into librenderer. Shows the scene's skybox (set 0, binding 2) as it is: no lighting, no tone mapping,
// so the sky looks like the images it was made from.
layout(location = 0) in vec3 direction;
layout(location = 0) out vec4 outColor;
layout(set = 0, binding = 2) uniform samplerCube skybox;

void main() {
    outColor = vec4(texture(skybox, direction).rgb, 1.0);
}
