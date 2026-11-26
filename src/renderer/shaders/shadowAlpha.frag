#version 450
// Built into librenderer (lighting/shadows.cpp): the shadow pass for alpha-masked materials. Cut-outs don't cast,
// using the same base colour alpha and cutoff as nreMaterialSurface.
#include "nre/lighting.glsl"

layout(location = 0) in vec2 inUV;

void main() {
    float alpha = texture(nreBaseColorTexture, inUV).a * nreMaterial.baseColorFactor.a;
    if (alpha < nreMaterial.alphaCutoff) discard;
}
