#pragma once
#include <vulkan/vulkan_core.h>

#include "renderer.h"


namespace NRE {
    class DescriptorSetLayout {
        VkDescriptorSetLayout descriptorSetLayout;
    public:
        Result<void> createLayout(uint32_t binding, VkDescriptorType descriptorType, uint32_t descriptorCount, VkShaderStageFlags stageFlags);
        VkDescriptorSetLayout getDescriptorSetLayout() const {
            return descriptorSetLayout;
        };
    };

    class DescriptorSetPool {
        VkDescriptorPool descriptorSetPool;
        Result<void> initDescriptorSetPool();
        public:
        DescriptorSetPool() {
            initDescriptorSetPool();
        };
        Result<VkDescriptorSet> allocateDescriptorSet(DescriptorSetLayout descriptorSetLayout);
    };

    // Points a binding of an allocated set at a buffer. Only rewrite a set the GPU isn't currently using.
    Result<void> writeBufferDescriptor(VkDescriptorSet set, uint32_t binding, VkBuffer buffer, VkDeviceSize size,
                                       VkDeviceSize offset = 0,
                                       VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

}
