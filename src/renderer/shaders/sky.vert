#version 450
// Built into librenderer (drawn by Sky::draw). One triangle that covers the whole screen, at the far plane
// (reverse-Z: depth 0): with depthCompare GREATER_OR_EQUAL it only fills the pixels no geometry was drawn on.
layout(push_constant) uniform PC {
    mat4 rayMatrix;   // mat3 inside: (ndc.x, ndc.y, 1) -> world-space view direction. Rotation only, so the sky never moves closer.
    vec4 params;      // x: 1 / exposure, y: the angle one pixel covers at the centre of the view (radians)
} pc;
layout(location = 0) out vec3 direction;
layout(location = 1) flat out vec4 params;   // push constants only reach the vertex stage

void main() {
    vec2 ndc = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2) * 2.0 - 1.0;   // (-1,-1), (3,-1), (-1,3)
    gl_Position = vec4(ndc, 0.0, 1.0);
    // Linear in ndc, so it interpolates correctly; normalized per pixel where the length matters.
    direction = mat3(pc.rayMatrix) * vec3(ndc, 1.0);
    params = pc.params;
}
