#include "pipeline/pipeline.h"

#include <iterator>

#include "core/context.h"
#include "core/errorHandling.h"

namespace NRE {
    Result<void> Pipeline::createPipeline(Handel<Shader> vertexShader, Handel<Shader> fragmentShader, const VertexLayout& layout,
                                          VkCompareOp depthCompare, bool depthWrite, VkFormat colorFormat) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        auto vertex = c->manager.get(vertexShader);
        if (!vertex) {
            auto error = ErrorHandeling::badHandle("vertex shader", vertexShader, "createShader()", std::source_location::current());
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            return {};   // no pipeline without shaders
        }
        auto fragment = c->manager.get(fragmentShader);
        if (!fragment) {
            auto error = ErrorHandeling::badHandle("fragment shader", fragmentShader, "createShader()", std::source_location::current());
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            return {};
        }
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr,0, VK_SHADER_STAGE_VERTEX_BIT, vertex->shader, "main"};
        stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr,0, VK_SHADER_STAGE_FRAGMENT_BIT, fragment->shader, "main"};

        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vi.vertexBindingDescriptionCount = static_cast<uint32_t>(layout.bindings.size());
        vi.pVertexBindingDescriptions = layout.bindings.data();
        vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(layout.attributes.size());
        vi.pVertexAttributeDescriptions = layout.attributes.data();

        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        vp.viewportCount = 1;
        vp.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rs.polygonMode = VK_POLYGON_MODE_FILL;

        rs.cullMode = VK_CULL_MODE_NONE;   // dynamic: set with vkCmdSetCullMode before drawing
        rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rs.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState cba{};
        cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        cb.attachmentCount = 1;
        cb.pAttachments = &cba;

        // Cull mode: per mesh, from its material (double-sided ones aren't culled). Core in Vulkan 1.3.
        VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_CULL_MODE};
        VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dyn.dynamicStateCount = uint32_t(std::size(dynStates));
        dyn.pDynamicStates = dynStates;

        VkPushConstantRange pcr{};
        pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pcr.offset     = 0;
        pcr.size       = sizeof(PushConstants);

        VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};

        std::vector<VkDescriptorSetLayout> descriptorLayouts;
        descriptorLayouts.reserve(c->vulkan.descriptorLayouts.size());

        for (auto& l : c->vulkan.descriptorLayouts) {
            descriptorLayouts.push_back(l.getDescriptorSetLayout());
        }

        plci.setLayoutCount = descriptorLayouts.size();
        plci.pSetLayouts    = descriptorLayouts.data();

        plci.pPushConstantRanges = &pcr;
        plci.pushConstantRangeCount = 1;

        if (VkResult r = vkCreatePipelineLayout(c->vulkan.device, &plci, nullptr, &pipelineLayout); r != VK_SUCCESS) {
            pipelineLayout = VK_NULL_HANDLE;
            auto error = Error{ErrorCode::ErrorCreatingPipeline, "createPipeline(): failed to create the pipeline layout: "
                               + std::string(ErrorHandeling::vkResultName(r))};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            return {};
        }

        VkPipelineRenderingCreateInfo rci{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
        rci.colorAttachmentCount = 1;
        rci.pColorAttachmentFormats = &colorFormat;
        rci.depthAttachmentFormat = depthFormat;


        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        ds.depthTestEnable = VK_TRUE;
        ds.depthWriteEnable = depthWrite ? VK_TRUE : VK_FALSE;
        ds.depthCompareOp = depthCompare;

        VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        gp.pNext = &rci;
        gp.stageCount = 2;
        gp.pStages = stages;
        gp.pVertexInputState = &vi;
        gp.pInputAssemblyState = &ia;
        gp.pViewportState = &vp;
        gp.pRasterizationState = &rs;
        gp.pMultisampleState = &ms;
        gp.pDepthStencilState = &ds;
        gp.pColorBlendState = &cb;
        gp.pDynamicState = &dyn;
        gp.layout = pipelineLayout;
        gp.renderPass = VK_NULL_HANDLE;    // dynamic rendering

        if (VkResult r = vkCreateGraphicsPipelines(c->vulkan.device, VK_NULL_HANDLE, 1, &gp, nullptr, &pipeline); r != VK_SUCCESS) {
            pipeline = VK_NULL_HANDLE;
            auto error = Error{ErrorCode::ErrorCreatingPipeline, "createPipeline(): vkCreateGraphicsPipelines failed: "
                               + std::string(ErrorHandeling::vkResultName(r)) + ". Run a Debug build: the validation layer says why."};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
        }
        return {};
    }
    Pipeline::~Pipeline() {
        auto c = context();
        if (!c) {
            ErrorHandeling::handleError(Error{ErrorCode::NotInitialized,"A Pipeline was destroyed after shutdown(): "
                                                                        "the Vulkan device it belongs to is already gone."});
            std::abort();
        }
        if (pipeline) vkDestroyPipeline(c->vulkan.device, pipeline, nullptr);
        if (pipelineLayout) vkDestroyPipelineLayout(c->vulkan.device, pipelineLayout, nullptr);
    }
}
