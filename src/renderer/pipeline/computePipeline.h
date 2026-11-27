#pragma once
#include <span>
#include <vulkan/vulkan_core.h>

#include "gpu/descriptorSet.h"
#include "renderer.h"

namespace NRE {
    // An internal compute pipeline (the sky's and the IBL's passes): its own descriptor set layout (set 0, from
    // the bindings given) and push constants, nothing of the scene's sets. Owned by the pass that uses it, which
    // calls destroy() in shutdown(), before the device goes.
    class ComputePipeline {
        DescriptorSetLayout setLayout;
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        uint32_t pushSize = 0;
    public:
        ComputePipeline() = default;
        ComputePipeline(const ComputePipeline&) = delete;
        ComputePipeline& operator=(const ComputePipeline&) = delete;
        ~ComputePipeline() { destroy(); }

        // spirv: a compute shader (entry point main). bindings: its set 0. pushConstantSize: bytes of its push
        // constant block, 0 for none (at most 128, the guaranteed minimum). Calling it again replaces the pipeline.
        Result<void> create(std::span<const uint32_t> spirv, std::span<const VkDescriptorSetLayoutBinding> bindings,
                            uint32_t pushConstantSize);
        // A set with this pipeline's layout, from the renderer's descriptor pool (freed with it at shutdown).
        Result<VkDescriptorSet> allocateSet() const;
        // The same from another pool (e.g. one its owner destroys to give the sets back).
        Result<VkDescriptorSet> allocateSet(VkDescriptorPool pool) const;
        // Binds the pipeline and set (as set 0), pushes pushConstantSize bytes from push (may be null when that's
        // 0) and dispatches x * y * z work groups. Outside a render pass; the caller places the barriers.
        void dispatch(VkCommandBuffer commandBuffer, VkDescriptorSet set, const void* push, uint32_t x, uint32_t y, uint32_t z) const;
        // False until create() succeeded (it may fail under RenderErrorPolicy::Ignore and leave it empty).
        bool ready() const { return pipeline != VK_NULL_HANDLE; }
        // Does nothing if it's empty.
        void destroy();
    };
}
