#include "vk_pipeline.h"
#include "vk_structs.h"
#include <stdexcept>

VkPipeline PipelineBuilder::build_pipeline(VkDevice device, VkRenderPass renderPass){ 
    //viewport and scissor rectangle needs to be combined into a viewport state using VkPipelineViewportStateCreateInfo struct
    VkPipelineViewportStateCreateInfo viewportState = vkStructs::pipeline_viewport_state_create_info(1, &viewport, 1, &scissor);
    //build the pipeline here

    VkPipelineColorBlendStateCreateInfo colourBlending = vkStructs::pipeline_colour_blend_state_create_info(1, &colourBlendAttachment);

    //finally we can begin bringing the pipeline together
    //we start by referencing the array of VkPipelineShaderStageCreateInfo structs
    VkGraphicsPipelineCreateInfo pipelineInfo = vkStructs::graphics_pipeline_create_info(2, shaderStages, &vertexInputInfo, &inputAssembly, &viewportState,
        &rasterizer, &multisampling, &depthStencil, &colourBlending, pipelineLayout, renderPass, 0);

    VkPipeline createdPipeline;
    
    //Now everything is in place we create the pipeline and store the reference in the class member
    if(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &createdPipeline) != VK_SUCCESS){
        throw std::runtime_error("Failed to create Graphics Pipeline");
    }

    return createdPipeline;
}
