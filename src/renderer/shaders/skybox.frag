#version 450
// Built into librenderer. Shows the scene's skybox (set 0, binding 2) without lighting. The cubemap is LDR
// (display values), so it's expanded with the inverse of the tone curve and divided by the exposure: after
// the tone-map pass the sky looks like the images it was made from, at any exposure.
layout(location = 0) in vec3 direction;
layout(location = 1) flat in float invExposure;
layout(location = 0) out vec4 outColor;
layout(set = 0, binding = 2) uniform samplerCube skybox;

// Inverse of Narkowicz's ACES fit. Must match inverseToneMapACES in lighting/lightingMath.h.
vec3 inverseToneMapACES(vec3 y) {
    y = clamp(y, 0.0, 0.999);   // the fit only reaches 1 at infinity
    vec3 a = 2.43 * y - 2.51;
    vec3 b = 0.59 * y - 0.03;
    vec3 c = 0.14 * y;
    vec3 disc = max(b * b - 4.0 * a * c, 0.0);
    return (-b - sqrt(disc)) / (2.0 * a);
}

void main() {
    outColor = vec4(inverseToneMapACES(texture(skybox, direction).rgb) * invExposure, 1.0);
}
