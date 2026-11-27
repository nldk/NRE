#include "gpu/descriptorSet.h"

#include <iterator>
#include <string>

#include "core/context.h"
#include "core/errorHandling.h"

namespace NRE {
    namespace {
        // Each scene allocates max_frames_in_flight frame sets (1 UBO + 1 SSBO each), and sets can't be freed
        // one by one, so this is also how many scenes can ever be created.
        constexpr uint32_t maxScenes = 8;
        constexpr uint32_t frameSets = max_frames_in_flight * maxScenes;
        constexpr uint32_t materialSets = 64;
        // The renderer's own passes, allocated with the material layout: the tone map's HDR input.
        constexpr uint32_t builtInSets = 1;
        // Every set with the material layout: 3 textures (base colour, metallic-roughness, normal) + 1 factor UBO.
        constexpr uint32_t materialLayoutSets = materialSets + builtInSets;
        constexpr uint32_t texturesPerMaterial = 3;
        // The built-in compute passes (ComputePipeline::allocateSet): the sky's LUT and cubemap passes and the BRDF
        // LUT's, room to spare. (Each Ibl allocates its passes' sets from its own pool.)
        // Each set has at most this many storage images and sampled images.
        constexpr uint32_t computeSets = 16;
        constexpr uint32_t imagesPerComputeSet = 2;
    }

    Result<void> DescriptorSetLayout::createLayout(std::span<const VkDescriptorSetLayoutBinding> bindings) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);

        VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        ci.bindingCount = static_cast<uint32_t>(bindings.size());
        ci.pBindings    = bindings.data();
        if (VkResult r = vkCreateDescriptorSetLayout(c->vulkan.device, &ci, nullptr, &descriptorSetLayout); r != VK_SUCCESS) {
            descriptorSetLayout = VK_NULL_HANDLE;
            auto error = Error{ErrorCode::DescriptorCreationFailed, "Failed to create a descriptor set layout with "
                               + std::to_string(bindings.size()) + " binding(s): " + std::string(ErrorHandeling::vkResultName(r))};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
        }
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
        NRE_REQUIRE_CONTEXT(c);
        VkDescriptorPoolSize sizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         frameSets + materialLayoutSets},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         frameSets},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, texturesPerMaterial * materialLayoutSets + 5 * frameSets   // frame sets: sky, shadow map, IBL x 3
                                                    + imagesPerComputeSet * computeSets},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          imagesPerComputeSet * computeSets},
        };
        VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pci.maxSets       = frameSets + materialLayoutSets + computeSets;
        pci.poolSizeCount = uint32_t(std::size(sizes));
        pci.pPoolSizes    = sizes;
        if (VkResult r = vkCreateDescriptorPool(c->vulkan.device, &pci, nullptr, &descriptorSetPool); r != VK_SUCCESS) {
            descriptorSetPool = VK_NULL_HANDLE;
            auto error = Error{ErrorCode::DescriptorCreationFailed, "init(): failed to create the descriptor pool: "
                               + std::string(ErrorHandeling::vkResultName(r))};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
        }
        return {};
    }
    Result<VkDescriptorSet> DescriptorSetPool::allocateDescriptorSet(DescriptorSetLayout descriptorSetLayout) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        auto layout = descriptorSetLayout.getDescriptorSetLayout();
        VkDescriptorSet set = VK_NULL_HANDLE;
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool     = descriptorSetPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &layout;
        if (VkResult r = vkAllocateDescriptorSets(c->vulkan.device, &ai, &set); r != VK_SUCCESS) {
            std::string message = "Failed to allocate a descriptor set: " + std::string(ErrorHandeling::vkResultName(r));
            if (r == VK_ERROR_OUT_OF_POOL_MEMORY || r == VK_ERROR_FRAGMENTED_POOL) {
                message += ". The descriptor pool is full: it holds " + std::to_string(materialSets) + " materials and "
                           + std::to_string(maxScenes) + " scenes, and destroyed ones don't give their sets back. "
                           "Raise the sizes in DescriptorSetPool::initDescriptorSetPool().";
            }
            auto error = Error{ErrorCode::DescriptorCreationFailed, std::move(message)};
            ErrorHandeling::handleError(error);
            return std::unexpected(error);   // there's no usable set to hand back, whatever the policy
        }
        return {set};
    }

    Result<void> writeBufferDescriptor(VkDescriptorSet set, uint32_t binding, VkBuffer buffer, VkDeviceSize size,
                                       VkDeviceSize offset, VkDescriptorType type) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);

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
        NRE_REQUIRE_CONTEXT(c);

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
    Result<void> writeStorageImageDescriptor(VkDescriptorSet set, uint32_t binding, VkImageView view) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);

        VkDescriptorImageInfo ii{VK_NULL_HANDLE, view, VK_IMAGE_LAYOUT_GENERAL};

        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet          = set;
        w.dstBinding      = binding;
        w.descriptorCount = 1;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        w.pImageInfo      = &ii;
        vkUpdateDescriptorSets(c->vulkan.device, 1, &w, 0, nullptr);
        return {};
    }

    Result<void> DescriptorSetPool::destroy() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);

        vkDestroyDescriptorPool(c->vulkan.device, descriptorSetPool, nullptr);

        return {};
    }

    Result<void> DescriptorSetLayout::destroy() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);

        vkDestroyDescriptorSetLayout(c->vulkan.device, descriptorSetLayout, nullptr);

        return {};
    }
}
