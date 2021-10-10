#pragma once
#include "vk_types.h"

namespace Vk{
	class PipelineBuilder {
	public:

		VkViewport viewport;
		VkRect2D scissor;
		
		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

		VkPipelineVertexInputStateCreateInfo vertexInputInfo;
		VkPipelineInputAssemblyStateCreateInfo inputAssembly;
		
		VkPipelineRasterizationStateCreateInfo rasterizer;
		VkPipelineMultisampleStateCreateInfo multisampling;
		VkPipelineDepthStencilStateCreateInfo depthStencil;
		VkPipelineColorBlendAttachmentState colourBlendAttachment;

		VkPipelineLayout pipelineLayout;

		//build pipeline call so we can create multiple pipelines
		VkPipeline build_pipeline(VkDevice device, VkRenderPass pass, uint32_t numShaderStages);
	};
}