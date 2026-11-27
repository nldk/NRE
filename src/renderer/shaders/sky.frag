#version 450
// Built into librenderer (drawn by Sky::draw): the scene's sky behind everything, from nreSky (set 0 binding 2).
//   skyParams.x 1, a cubemap: LDR display values, expanded with the inverse of the tone curve and divided by the
//                  exposure, so after the tone-map pass it looks like the images it was made from, at any exposure.
//   skyParams.x 2, procedural: linear HDR from the sky cubemap (Sky::update, the finished sky radiance),
//                  plus the sun's disk, then the haze near the horizon (the same as on objects, so far terrain
//                  and the sky meet without a seam). Goes through the exposure like the lit scene does.
// A cubemap sky is shown as authored, without haze: a photo already has its own.
#include "nre/lighting.glsl"   // for the haze; only set 0 is used
#include "atmosphere.glsl"
#include "toneCurve.glsl"

layout(location = 0) in vec3 direction;
layout(location = 1) flat in vec4 params;   // x: 1 / exposure, y: the angle one pixel covers (radians)
layout(location = 0) out vec4 outColor;

void main() {
    if (nreFrame.skyParams.x == 1.0) {
        outColor = vec4(inverseToneMapACES(texture(nreSky, direction).rgb) * params.x, 1.0);
        return;
    }
    vec3 dir = normalize(direction);
    vec3 color = textureLod(nreSky, dir, 0.0).rgb;

    // The sun's disk: sunRadiance (the light it gives, after the atmosphere and the horizon fade) spread over its
    // solid angle. Its edge covers a pixel partially, so it's blended by coverage. Hidden behind the ground.
    vec3 toSun = -nreFrame.sunDirection.xyz;
    float angle = 2.0 * asin(min(0.5 * length(dir - toSun), 1.0));   // precise for small angles, unlike acos
    float coverage = clamp((ATM_sunAngularRadius - angle) / max(params.y, 1e-6) + 0.5, 0.0, 1.0);
    if (coverage > 0.0 && !atm_viewHitsGround(dir)) {
        float solidAngle = ATM_PI * ATM_sunAngularRadius * ATM_sunAngularRadius;
        color += coverage * nreFrame.sunRadiance.rgb / solidAngle;
    }
    if (nreFrame.hazeParams.x > 0.0) color = mix(nre_hazeColor(dir), color, nre_hazeSkyTransmittance(dir));
    // The HDR target is half float: keep the disk below its largest value (65504) so the tone map never sees inf.
    outColor = vec4(min(color, vec3(60000.0)), 1.0);
}
