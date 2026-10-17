#include "descriptorSet.h"

#include "context.h"

namespace NRE {
    Result<void> DescriptorSetLayout::createLayout(uint32_t binding, VkDescriptorType descriptorType, uint32_t descriptorCount, VkShaderStageFlags stageFlags) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");

        VkDescriptorSetLayoutBinding b{};
        b.binding         = binding;
        b.descriptorType  = descriptorType;
        b.descriptorCount = descriptorCount;
        b.stageFlags      = stageFlags;

        VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        ci.bindingCount = 1;
        ci.pBindings    = &b;
        vkCreateDescriptorSetLayout(c->vulkan.device, &ci, nullptr, &descriptorSetLayout);
        return {};
    }

    Result<void> DescriptorSetPool::initDescriptorSetPool() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
        VkDescriptorPoolSize sizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         max_frames_in_flight},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64},
        };
        VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pci.maxSets       = max_frames_in_flight + 64;
        pci.poolSizeCount = 2;
        pci.pPoolSizes    = sizes;
        vkCreateDescriptorPool(c->vulkan.device, &pci, nullptr, &descriptorSetPool);
        return {};
    }
    Result<VkDescriptorSet> DescriptorSetPool::allocateDescriptorSet(DescriptorSetLayout descriptorSetLayout) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
        auto layout = descriptorSetLayout.getDescriptorSetLayout();
        VkDescriptorSet set;
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool     = descriptorSetPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &layout;
        vkAllocateDescriptorSets(c->vulkan.device, &ai, &set);

        return {set};
    }

    Result<void> writeBufferDescriptor(VkDescriptorSet set, uint32_t binding, VkBuffer buffer, VkDeviceSize size,
                                       VkDeviceSize offset, VkDescriptorType type) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");

        VkDescriptorBufferInfo bi{buffer, offset, size};

        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet          = set;
        w.dstBinding      = binding;
        w.descriptorCount = 1;
        w.descriptorType  = type;
        w.pBufferInfo     = &bi;
        vkUpdateDescriptorSets(c->vulkan.device, 1, &w, 0, nullptr);
        return {};
    }
}
