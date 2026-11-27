#version 450
// Built into librenderer. The last pass of the frame: reads the linear HDR scene and writes it to the window
// (or the unsafe render target) in the swapchain format, which does the sRGB encoding.
layout(location = 0) flat in vec4 params;   // x: exposure, y: 1 = copy without tone mapping (Render_2D)
layout(location = 0) out vec4 outColor;
layout(set = 1, binding = 0) uniform sampler2D hdr;   // same size as the output, so pixels map 1:1

// Narkowicz's ACES fit. Must match toneMapACES in lighting/lightingMath.h.
vec3 toneMapACES(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
    vec3 color = texelFetch(hdr, ivec2(gl_FragCoord.xy), 0).rgb;
    // The half-float target holds +inf for anything past 65504 (a very smooth highlight, a tiny exposure), and ACES of
    // inf is NaN: keep inf finite and drop NaN to black.
    color = mix(color, vec3(0.0), isnan(color));
    color = min(color, vec3(65504.0));
    outColor = vec4(params.y != 0.0 ? color : toneMapACES(color * params.x), 1.0);
}
