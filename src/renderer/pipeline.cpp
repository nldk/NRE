#include "pipeline.h"

#include "context.h"

namespace NRE {
    Result<void> Pipeline::createPipeline(Handel<Shader> vertexShader, Handel<Shader> fragmentShader,const VertexLayout& layout = {}) {
        auto c = context();
        if (!c) {
            if (ErrorHandeling::handleError(Error{ErrorCode::NotInitialized,"Renderer not initialized"})) {
                return std::unexpected(Error{ErrorCode::NotInitialized,"Renderer not initialized"});
            };
        }
        auto vertex = c->manager.get(vertexShader);
        if (!vertex) {
            if (ErrorHandeling::handleError(Error{ErrorCode::HandelOutOfDateOrNonExistend,"Handel to vertex shader out of date or non-existing"})) {
                return std::unexpected(Error{ErrorCode::HandelOutOfDateOrNonExistend,"Handel to vertex shader out of date or non-existing"});
            };
        }
        auto fragment = c->manager.get(fragmentShader);
        if (!vertex) {
            if (ErrorHandeling::handleError(Error{ErrorCode::HandelOutOfDateOrNonExistend,"Handel to fragment shader out of date or non-existing"})) {
                return std::unexpected(Error{ErrorCode::HandelOutOfDateOrNonExistend,"Handel to fragment shader out of date or non-existing"});
            };
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

        rs.cullMode = VK_CULL_MODE_BACK_BIT;
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

        VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dyn.dynamicStateCount = 2;
        dyn.pDynamicStates = dynStates;

        VkPushConstantRange pcr{};
        pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pcr.offset     = 0;
        pcr.size       = sizeof(PushConstants);

        VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        plci.pPushConstantRanges = &pcr;
        plci.pushConstantRangeCount = 1;

        vkCreatePipelineLayout(c->vulkan.device, &plci, nullptr, &pipelineLayout);

        VkFormat colorFormat = c->vulkan.swapchain.image_format;
        VkPipelineRenderingCreateInfo rci{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
        rci.colorAttachmentCount = 1;
        rci.pColorAttachmentFormats = &colorFormat;
        rci.depthAttachmentFormat = depthFormat;


        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        ds.depthTestEnable = VK_TRUE;
        ds.depthWriteEnable = VK_TRUE;
        ds.depthCompareOp = VK_COMPARE_OP_LESS;

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

        VkResult r = vkCreateGraphicsPipelines(c->vulkan.device, VK_NULL_HANDLE, 1, &gp, nullptr, &pipeline);
        return {};
    }
    Pipeline::~Pipeline() {
        auto c = context();
        if (!c) {
            ErrorHandeling::handleError(Error{ErrorCode::NotInitialized,"Renderer not initialized"});
            std::abort();
        }
        if (pipeline) vkDestroyPipeline(c->vulkan.device, pipeline, nullptr);
        if (pipelineLayout) vkDestroyPipelineLayout(c->vulkan.device, pipelineLayout, nullptr);
    }
}
