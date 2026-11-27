// Built into librenderer: helpers for the compute passes that write cubemaps (skyCubemap.comp, the IBL passes).
#ifndef NRE_CUBE_FACES_GLSL
#define NRE_CUBE_FACES_GLSL

// The direction through texel st (-1..1, from the face's top left) of a cube face, in Vulkan's face order and
// orientation (+X, -X, +Y, -Y, +Z, -Z), as a samplerCube looks it up. Not normalized.
vec3 faceDirection(int face, vec2 st) {
    switch (face) {
        case 0: return vec3(1.0, -st.y, -st.x);
        case 1: return vec3(-1.0, -st.y, st.x);
        case 2: return vec3(st.x, 1.0, st.y);
        case 3: return vec3(st.x, -1.0, -st.y);
        case 4: return vec3(st.x, -st.y, 1.0);
        default: return vec3(-st.x, -st.y, -1.0);
    }
}

// The unit direction through the centre of texel p (x, y, face) of a size x size face.
vec3 texelDirection(ivec3 p, int size) {
    vec2 st = (vec2(p.xy) + 0.5) / float(size) * 2.0 - 1.0;
    return normalize(faceDirection(p.z, st));
}

#endif
