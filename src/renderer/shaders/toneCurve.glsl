// Built into librenderer: the tone curve's inverse, for LDR (display-referred) skies: sky.frag draws them with it,
// iblSource.comp expands them with it before the IBL filters them.
#ifndef NRE_TONE_CURVE_GLSL
#define NRE_TONE_CURVE_GLSL

// Inverse of Narkowicz's ACES fit. Must match inverseToneMapACES in lighting/lightingMath.h.
vec3 inverseToneMapACES(vec3 y) {
    y = clamp(y, 0.0, 0.999);   // the fit reaches 1 at x ~ 7.24 and is clamped beyond it: no finite inverse at 1
    vec3 a = 2.43 * y - 2.51;
    vec3 b = 0.59 * y - 0.03;
    vec3 c = 0.14 * y;
    vec3 disc = max(b * b - 4.0 * a * c, 0.0);
    return (-b - sqrt(disc)) / (2.0 * a);
}

#endif
