#pragma once
#include <vulkan/vulkan_core.h>

#include "renderer.h"


namespace NRE {
    class DescriptorSetLayout {
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    public:
        Result<void> createLayout(uint32_t binding, VkDescriptorType descriptorType, uint32_t descriptorCount, VkShaderStageFlags stageFlags);
        VkDescriptorSetLayout getDescriptorSetLayout() const {
            return descriptorSetLayout;
        };
        Result<void> destroy();
    };

    class DescriptorSetPool {
        VkDescriptorPool descriptorSetPool = VK_NULL_HANDLE;
    public:
        Result<void> initDescriptorSetPool();
        // Doesn't create the pool: Context is constructed before the device exists. Call initDescriptorSetPool() in init().
        DescriptorSetPool() = default;
        Result<VkDescriptorSet> allocateDescriptorSet(DescriptorSetLayout descriptorSetLayout);
        Result<void> destroy();
    };

    // Points a binding of an allocated set at a buffer. Only rewrite a set the GPU isn't currently using.
    Result<void> writeBufferDescriptor(VkDescriptorSet set, uint32_t binding, VkBuffer buffer, VkDeviceSize size,
                                       VkDeviceSize offset = 0,
                                       VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    // Points a combined-image-sampler binding at an image view + sampler. `layout` is the layout the image
    // will be in when a shader samples it, not a transition. Only rewrite a set the GPU isn't currently using.
    Result<void> writeImageDescriptor(VkDescriptorSet set, uint32_t binding, VkImageView view, VkSampler sampler,
                                      VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

}
