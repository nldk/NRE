#version 450
// Built into librenderer. One triangle that covers the whole screen, for full-screen passes (tone mapping).
// Push constants only reach the vertex stage, so it hands them to the fragment stage as a flat varying.
layout(push_constant) uniform PC {
    vec4 params;   // what it means is up to the fragment shader
} pc;
layout(location = 0) flat out vec4 params;

void main() {
    vec2 ndc = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2) * 2.0 - 1.0;   // (-1,-1), (3,-1), (-1,3)
    gl_Position = vec4(ndc, 0.0, 1.0);
    params = pc.params;
}
