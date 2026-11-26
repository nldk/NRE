#version 450
// Physically based lighting (the metallic-roughness model glTF uses): GGX specular + Lambert diffuse.
// Color math is linear: the texture is SRGB (decoded on sampling), and the output is linear HDR light that
// the renderer tone maps (with the scene's exposure) at the end of the frame, so don't tone map here.
layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inWorldPos;
layout(location = 0) out vec4 outColor;

// Must match GpuLight / FrameData in gpu/UBOAndSSBO.h.
struct Light {
    vec4 position;   // w = 0: directional, xyz = direction the light travels. w = 1: point light, xyz = position
    vec4 color;      // rgb = color, w = intensity
};
layout(set = 0, binding = 0) uniform Frame {
    mat4 viewProj;
    vec4 cameraPos;
    uint lightCount;
} frame;
layout(set = 0, binding = 1) readonly buffer Lights { Light lights[]; };
layout(set = 1, binding = 0) uniform sampler2D baseTexture;   // set 1 = material

const float PI = 3.14159265;

// D: how many microfacets face the half vector. a = roughness².
float distributionGGX(float NdotH, float a) {
    float a2 = a * a;
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}
// V: microfacets hidden by their neighbours (height-correlated Smith, as in Filament).
float visibilitySmith(float NdotV, float NdotL, float a) {
    float a2 = a * a;
    float v = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
    float l = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
    return 0.5 / (v + l);
}
// F: reflection grows toward grazing angles.
vec3 fresnelSchlick(float VdotH, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
}
void main() {
    vec3 albedo = texture(baseTexture, inUV).rgb;
    float metallic = 0.0;    // later: from the material (glTF metallicRoughness texture/factors)
    float roughness = 0.5;
    float a = roughness * roughness;   // artists' roughness is "perceptual"; the math wants it squared

    vec3 N = normalize(inNormal);                          // interpolation shortens normals
    vec3 V = normalize(frame.cameraPos.xyz - inWorldPos);  // towards the eye
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    float NdotV = max(dot(N, V), 1e-4);

    vec3 color = vec3(0.0);
    for (uint i = 0; i < frame.lightCount; ++i) {
        Light light = lights[i];
        vec3 L;
        float attenuation = 1.0;
        if (light.position.w == 0.0) {
            L = normalize(-light.position.xyz);
        } else {
            vec3 toLight = light.position.xyz - inWorldPos;
            float dist2 = max(dot(toLight, toLight), 1e-4);
            L = toLight * inversesqrt(dist2);
            attenuation = 1.0 / dist2;                     // inverse-square falloff, like real light
        }
        float NdotL = dot(N, L);
        if (NdotL <= 0.0) continue;                        // light is behind the surface

        vec3 H = normalize(V + L);                         // half vector between eye and light
        float NdotH = max(dot(N, H), 0.0);
        float VdotH = max(dot(V, H), 0.0);

        vec3 F = fresnelSchlick(VdotH, F0);
        vec3 specular = distributionGGX(NdotH, a) * visibilitySmith(NdotV, NdotL, a) * F;
        vec3 diffuse = (1.0 - F) * (1.0 - metallic) * albedo / PI;   // what isn't reflected scatters; metals don't

        vec3 radiance = light.color.rgb * light.color.w * attenuation;
        color += (diffuse + specular) * radiance * NdotL;
    }

    // Ambient: stand-in for light bouncing around the scene. Sky-ish from above, darker from below.
    vec3 ambient = mix(vec3(0.02, 0.02, 0.025), vec3(0.06, 0.07, 0.09), N.y * 0.5 + 0.5);
    color += ambient * albedo * (1.0 - metallic * 0.5);

    outColor = vec4(color, 1.0);
}
