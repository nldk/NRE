#pragma once
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

#include "buffer.h"
#include "renderer.h"
#include "core/frames.h"

namespace NRE {
    // Same layout as the shader's structs (shaders/include/nre/frame.glsl and lighting.glsl): only vec4/mat4 and
    // 4-byte scalars with explicit padding, so std140/std430 add no hidden padding.
    struct alignas(16) GpuLight  { glm::vec4 position; glm::vec4 color; };
    struct alignas(16) FrameData {
        glm::mat4 viewProj;
        glm::vec4 cameraPos;
        uint32_t lightCount;
        uint32_t padding[3];      // std140 starts the next vec4 at 96; glm's vec4 is only 4-byte aligned
        // Everything below is appended, so shaders that declare only the start of the block still match.
        glm::vec4 sunDirection;   // xyz: the way the sunlight travels (normalized), w unused
        glm::vec4 sunColor;       // rgb: color, w: intensity. 0 when the scene has no sun
        glm::mat4 cascadeViewProj[4];   // world -> shadow clip, per cascade
        glm::vec4 cascadeSplits;  // view-space distance where each cascade ends
        glm::vec4 shadowParams;   // x: cascade count (0 = no sun shadows), y: normal offset in shadow-map texels (of the cascade used), zw: 0
        glm::vec4 sunRadiance;    // rgb: light reaching objects (sun colour x intensity x atmosphere tint), w: 1 if the sun is up
        glm::vec4 skyColor;       // rgb: the sky colour expanded with inverseToneMapACES (linear), w: 0
        glm::vec4 skyParams;      // x: sky source (0 colour, 1 cubemap, 2 procedural), y: specular mip count,
                                  // z: procedural sky brightness = sun intensity / 3 (its cubemap holds the sky, night
                                  // floor included, at intensity 3; this scales both), else 0. w: 0
        glm::vec4 hazeParams;     // x: density per metre (0 = off), y: 1 / haze height, zw: 0
    };
    static_assert(sizeof(GpuLight) == 32);
    static_assert(offsetof(FrameData, viewProj) == 0 && offsetof(FrameData, cameraPos) == 64);
    static_assert(offsetof(FrameData, lightCount) == 80 && offsetof(FrameData, sunDirection) == 96);
    static_assert(offsetof(FrameData, sunColor) == 112 && offsetof(FrameData, cascadeViewProj) == 128);
    static_assert(offsetof(FrameData, cascadeSplits) == 384 && offsetof(FrameData, shadowParams) == 400);
    static_assert(offsetof(FrameData, sunRadiance) == 416 && offsetof(FrameData, skyColor) == 432);
    static_assert(offsetof(FrameData, skyParams) == 448 && offsetof(FrameData, hazeParams) == 464);
    static_assert(sizeof(FrameData) == 480);

    // A material's factors (set 1, binding 3). Multiplied with its textures in nreMaterialSurface.
    struct alignas(16) GpuMaterial {
        static constexpr uint32_t alphaMaskFlag = 1u << 0;     // discard where alpha < alphaCutoff
        static constexpr uint32_t doubleSidedFlag = 1u << 1;   // back faces are drawn, with the normal flipped
        glm::vec4 baseColorFactor;
        float metallic, roughness, alphaCutoff, normalScale;
        uint32_t flags;
        uint32_t pad[3];
    };
    static_assert(offsetof(GpuMaterial, metallic) == 16 && offsetof(GpuMaterial, flags) == 32);
    static_assert(sizeof(GpuMaterial) == 48);

// A UBO or SSBO the CPU rewrites every frame: one copy per frame in flight, so the CPU never writes
// a copy the GPU is still reading. Call destroy() in shutdown(), before the allocator is destroyed.
class PerFrameBuffer {
    Buffer buffers[max_frames_in_flight];
    VkDescriptorSet sets[max_frames_in_flight]{};      // VK_NULL_HANDLE until attach()
    VkDeviceSize capacities[max_frames_in_flight]{};   // per frame: each copy grows on its own
    uint32_t binding = 0;
    VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    // Replaces frame's copy with one of at least needed bytes and repoints its descriptor.
    Result<void> grow(uint32_t frame, VkDeviceSize needed);
public:
    // type is VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER or VK_DESCRIPTOR_TYPE_STORAGE_BUFFER. Calling it again replaces the buffers.
    Result<void> create(VkDescriptorType type, VkDeviceSize size);
    // Points binding in each frame's set at that frame's copy. Only while no submitted frame uses the sets (e.g. in init()).
    Result<void> attach(const VkDescriptorSet (&frameSets)[max_frames_in_flight], uint32_t binding);
    // Copies into frame's copy. Call it after waiting on that frame's fence.
    // A storage buffer grows when size doesn't fit; a uniform buffer can't.
    Result<void> write(uint32_t frame, const void* data, VkDeviceSize size);
    // Does nothing if it was never created.
    Result<void> destroy();

    [[nodiscard]] bool created() const { return buffers[0].buffer != VK_NULL_HANDLE; }
    [[nodiscard]] VkBuffer getBuffer(uint32_t frame) const { return buffers[frame].buffer; }
    [[nodiscard]] VkDeviceSize getCapacity(uint32_t frame) const { return capacities[frame]; }
    [[nodiscard]] VkDescriptorType getType() const { return type; }
};
}
