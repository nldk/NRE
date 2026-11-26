#include "pipeline/computePipeline.h"

#include <string>

#include "core/context.h"
#include "core/errorHandling.h"

namespace NRE {
    Result<void> ComputePipeline::create(std::span<const uint32_t> spirv, std::span<const VkDescriptorSetLayoutBinding> bindings,
                                         uint32_t pushConstantSize) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        destroy();
        VkDevice device = c->vulkan.device;
        auto fail = [&](const char* what, VkResult r) -> Result<void> {
            destroy();
            auto error = Error{ErrorCode::ErrorCreatingPipeline, std::string("Failed to create a built-in compute pipeline: ")
                               + what + ": " + std::string(ErrorHandeling::vkResultName(r))};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            return {};
        };

        if (auto r = setLayout.createLayout(bindings); !r) {
            destroy();
            return r;
        }
        if (setLayout.getDescriptorSetLayout() == VK_NULL_HANDLE) return {};   // Ignore policy: already reported

        VkDescriptorSetLayout setLayoutHandle = setLayout.getDescriptorSetLayout();
        VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT, 0, pushConstantSize};
        VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        plci.setLayoutCount = 1;
        plci.pSetLayouts = &setLayoutHandle;
        plci.pushConstantRangeCount = pushConstantSize > 0 ? 1 : 0;
        plci.pPushConstantRanges = &range;
        if (VkResult r = vkCreatePipelineLayout(device, &plci, nullptr, &layout); r != VK_SUCCESS) {
            layout = VK_NULL_HANDLE;
            return fail("vkCreatePipelineLayout", r);
        }
        pushSize = pushConstantSize;

        VkShaderModuleCreateInfo smci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        smci.codeSize = spirv.size_bytes();
        smci.pCode = spirv.data();
        VkShaderModule module = VK_NULL_HANDLE;
        if (VkResult r = vkCreateShaderModule(device, &smci, nullptr, &module); r != VK_SUCCESS) {
            return fail("vkCreateShaderModule", r);
        }
        VkComputePipelineCreateInfo cpci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        cpci.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, module, "main"};
        cpci.layout = layout;
        VkResult r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &cpci, nullptr, &pipeline);
        vkDestroyShaderModule(device, module, nullptr);   // the pipeline doesn't need it once made
        if (r != VK_SUCCESS) {
            pipeline = VK_NULL_HANDLE;
            return fail("vkCreateComputePipelines", r);
        }
        return {};
    }

    Result<VkDescriptorSet> ComputePipeline::allocateSet() const {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        return c->vulkan.descriptorPool.allocateDescriptorSet(setLayout);
    }

    void ComputePipeline::dispatch(VkCommandBuffer cmd, VkDescriptorSet set, const void* push, uint32_t x, uint32_t y, uint32_t z) const {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &set, 0, nullptr);
        if (pushSize > 0 && push) vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, pushSize, push);
        vkCmdDispatch(cmd, x, y, z);
    }

    void ComputePipeline::destroy() {
        if (pipeline == VK_NULL_HANDLE && layout == VK_NULL_HANDLE && setLayout.getDescriptorSetLayout() == VK_NULL_HANDLE) return;
        auto c = context();
        if (!c) return;   // the device is gone, and everything made on it with it
        if (pipeline) vkDestroyPipeline(c->vulkan.device, pipeline, nullptr);
        if (layout) vkDestroyPipelineLayout(c->vulkan.device, layout, nullptr);
        if (setLayout.getDescriptorSetLayout() != VK_NULL_HANDLE) (void)setLayout.destroy();
        pipeline = VK_NULL_HANDLE;
        layout = VK_NULL_HANDLE;
        setLayout = {};
        pushSize = 0;
    }
}
