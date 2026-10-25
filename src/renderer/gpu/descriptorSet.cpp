#include "gpu/descriptorSet.h"

#include "core/context.h"

namespace NRE {
    Result<void> DescriptorSetLayout::createLayout(std::span<const VkDescriptorSetLayoutBinding> bindings) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");

        VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        ci.bindingCount = static_cast<uint32_t>(bindings.size());
        ci.pBindings    = bindings.data();
        vkCreateDescriptorSetLayout(c->vulkan.device, &ci, nullptr, &descriptorSetLayout);
        return {};
    }

    Result<void> DescriptorSetLayout::createLayout(uint32_t binding, VkDescriptorType descriptorType, uint32_t descriptorCount, VkShaderStageFlags stageFlags) {
        VkDescriptorSetLayoutBinding b{};
        b.binding         = binding;
        b.descriptorType  = descriptorType;
        b.descriptorCount = descriptorCount;
        b.stageFlags      = stageFlags;
        return createLayout(std::span(&b, 1));
    }

    Result<void> DescriptorSetPool::initDescriptorSetPool() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
        // Each scene allocates max_frames_in_flight frame sets (1 UBO + 1 SSBO each), and sets can't be freed
        // one by one, so this is also how many scenes can ever be created.
        constexpr uint32_t maxScenes = 8;
        constexpr uint32_t frameSets = max_frames_in_flight * maxScenes;
        constexpr uint32_t materialSets = 64;
        VkDescriptorPoolSize sizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         frameSets},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         frameSets},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, materialSets},
        };
        VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pci.maxSets       = frameSets + materialSets;
        pci.poolSizeCount = 3;
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

    Result<void> writeImageDescriptor(VkDescriptorSet set, uint32_t binding, VkImageView view, VkSampler sampler,
                                      VkImageLayout layout) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");

        VkDescriptorImageInfo ii{sampler, view, layout};

        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet          = set;
        w.dstBinding      = binding;
        w.descriptorCount = 1;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.pImageInfo      = &ii;
        vkUpdateDescriptorSets(c->vulkan.device, 1, &w, 0, nullptr);
        return {};
    }
    Result<void> DescriptorSetPool::destroy() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");

        vkDestroyDescriptorPool(c->vulkan.device, descriptorSetPool, nullptr);

        return {};
    }

    Result<void> DescriptorSetLayout::destroy() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");

        vkDestroyDescriptorSetLayout(c->vulkan.device, descriptorSetLayout, nullptr);

        return {};
    }
}
