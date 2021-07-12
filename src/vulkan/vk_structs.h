#pragma once
#include "vk_types.h"
#include "vk_mesh.h"
#include <set>

namespace vkStructs{

  VkSubmitInfo submit_info(VkCommandBuffer* commandBuffer);

  //VkSubmitInfo submit_info(VkCommandBuffer* cmd, VkSemaphore* waitSemaphores, uint32_t waitCount,
  //  VkPipelineStageFlags* waitStages, VkSemaphore* signalSemaphores, uint32_t signalCount
  //);
   VkSubmitInfo submit_info(VkCommandBuffer commandBuffers[], uint32_t cmdBufferCount, VkSemaphore* waitSemaphores, uint32_t waitCount,
    VkPipelineStageFlags* waitStages, VkSemaphore* signalSemaphores, uint32_t signalCount
  );

  VkPresentInfoKHR present_info(VkSemaphore* signalSemaphores, uint32_t waitCount, 
    VkSwapchainKHR* swapChains, uint32_t *imageIndex);

  VkApplicationInfo app_info(const char* name);

  VkInstanceCreateInfo instance_create_info(VkApplicationInfo &appInfo, std::vector<const char *> &extensions, const char* const * enabledLayerNames, uint32_t enabledLayerCount, void *next);

  std::vector<VkDeviceQueueCreateInfo> queue_create_infos(std::set<uint32_t> &uniqueQueueFamilies, const float* queuePriority);

  VkDeviceCreateInfo device_create_info(VkPhysicalDeviceFeatures& deviceFeatures, std::vector<VkDeviceQueueCreateInfo>& queueCreateInfos,const std::vector<const char*>& deviceExtensions);

  VmaAllocatorCreateInfo vma_allocator_info(VkPhysicalDevice& physicalDevice, VkDevice& device, VkInstance& instance, VmaAllocator& allocator);

  VkSwapchainCreateInfoKHR swapchain_create_info(VkSurfaceKHR& surface, uint32_t& imageCount, VkSurfaceFormatKHR& surfaceFormat, VkExtent2D& extent, 
    uint32_t queueFamilyIndices[2], VkSurfaceTransformFlagBitsKHR& currentTransform, VkPresentModeKHR& presentMode);

  VkAttachmentDescription attachment_description(VkFormat format, enum VkSampleCountFlagBits samples, enum VkAttachmentLoadOp loadOp, 
    enum VkAttachmentStoreOp storeOp, enum VkImageLayout initialLayout, enum VkImageLayout finalayout);

  VkAttachmentReference attachment_reference(uint32_t attachmentIndex, enum VkImageLayout layout);

  VkSubpassDependency subpass_dependency(uint32_t srcSubpass, uint32_t dstSubpass,VkPipelineStageFlags srcStageMask, VkAccessFlags srcAccessMask, 
    VkPipelineStageFlags dstStageMask, VkAccessFlags dstAccessMask);

  VkSubpassDescription subpass_description(VkPipelineBindPoint pipelineBindPoint, uint32_t colourAttachmentCount, 
    const VkAttachmentReference* pResolveAttachments, const VkAttachmentReference* pColorAttachments, const VkAttachmentReference* pDepthStencilAttachments);

  VkRenderPassCreateInfo renderpass_create_info(std::vector<VkAttachmentDescription>& attachments, uint32_t subpassCount, 
    VkSubpassDescription* pSubpasses,  uint32_t dependencyCount, VkSubpassDependency* pDependencies);

  VkDescriptorSetLayoutBinding descriptorset_layout_binding(uint32_t binding, enum VkDescriptorType type, uint32_t descriptorCount, VkShaderStageFlags stageFlags, VkSampler* pSampler);

  VkDescriptorSetLayoutCreateInfo descriptorset_layout_create_info(std::vector<VkDescriptorSetLayoutBinding>& bindings);

  VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(enum VkShaderStageFlagBits pipelineStage, VkShaderModule &shaderModule, 
    const VkSpecializationInfo* pSpecialInfo);

  //VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_create_info(uint32_t bindingDescriptionCount, const VkVertexInputBindingDescription* pBindingDescriptions, 
    //std::vector<VkVertexInputAttributeDescription>& attributeDescriptions);

  //VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_create_info(uint32_t bindingDescriptionCount, const VkVertexInputBindingDescription* pBindingDescriptions, 
  //  const VkVertexInputAttributeDescription* pVertexAttributeDescriptions,  uint32_t vertexAttributeDescriptionCount);

  VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_create_info(VertexInputDescription& vertexInputDescription);

  VkPipelineInputAssemblyStateCreateInfo pipeline_input_assembly_state_create_info(enum VkPrimitiveTopology topology, VkBool32 restartEnabled);

  VkViewport viewport(float x, float y, float width, float height, float minDepth, float maxDepth);

  VkRect2D scissor(int32_t offsetX, int32_t offsetY, VkExtent2D& extent);

  VkPipelineViewportStateCreateInfo pipeline_viewport_state_create_info(uint32_t viewportCount, const VkViewport* viewport, uint32_t scissorCount, const VkRect2D* scissor);

  VkPipelineRasterizationStateCreateInfo pipeline_rasterization_state_create_info(enum VkPolygonMode polygonMode, VkCullModeFlags cullMode,
    enum VkFrontFace frontFace, VkBool32 depthClamp = VK_FALSE, VkBool32 depthBiasEnable = VK_FALSE, float depthBiasConstantFactor = 0.0f, 
    float depthBiasClamp = 0.0f, float depthBiasSlopeFactor = 0.0f, VkBool32 discard = VK_FALSE);

  VkPipelineMultisampleStateCreateInfo pipeline_msaa_state_create_info(VkSampleCountFlagBits rasterizationSamples);

  VkPipelineDepthStencilStateCreateInfo pipeline_depth_stencil_state_create_info(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, enum VkCompareOp depthCompareOp,
    VkBool32 depthBoundsTestEnable = VK_FALSE, float minDepthBound = 0.0f, float maxDepthBound = 1.0f);

  VkPipelineColorBlendAttachmentState pipeline_colour_blend_attachment(VkColorComponentFlags colourComponentFlags, VkBool32 blendEnable = VK_FALSE, enum VkBlendFactor srcCBlendFactor = VK_BLEND_FACTOR_ONE, 
    enum VkBlendFactor dstCBlendFactor = VK_BLEND_FACTOR_ZERO, enum VkBlendFactor srcABlendFactor = VK_BLEND_FACTOR_ONE, enum VkBlendFactor dstABlendFactor = VK_BLEND_FACTOR_ZERO);

  VkPipelineColorBlendStateCreateInfo pipeline_colour_blend_state_create_info(uint32_t attachmentCount, const VkPipelineColorBlendAttachmentState* colourBlendAttachment);

  VkPipelineDynamicStateCreateInfo pipeline_dynamic_state_create_info(uint32_t dynamicStateCount, const VkDynamicState* dynamicStates);

  VkPipelineLayoutCreateInfo pipeline_layout_create_info(uint32_t setLayoutCount = 0, const VkDescriptorSetLayout* pSetLayouts = nullptr, 
    uint32_t pushConstRangeCount = 0, const VkPushConstantRange* pushConstantRanges = nullptr);

  VkGraphicsPipelineCreateInfo graphics_pipeline_create_info(uint32_t stageCount, const std::vector<VkPipelineShaderStageCreateInfo>& shaderStages, 
    const VkPipelineVertexInputStateCreateInfo* vertexInputInfo, const VkPipelineInputAssemblyStateCreateInfo* inputAssembly,
    const VkPipelineViewportStateCreateInfo* viewportState, const VkPipelineRasterizationStateCreateInfo* rasterizer,
    const VkPipelineMultisampleStateCreateInfo* multisampling, const VkPipelineDepthStencilStateCreateInfo* depthStencil,
    const VkPipelineColorBlendStateCreateInfo* colourBlending, const VkPipelineLayout& pipelineLayout, VkRenderPass& renderPass, uint32_t subpassIndex);

  VkShaderModuleCreateInfo shader_module_create_info(const std::vector<char>& code);

  VkFramebufferCreateInfo framebuffer_create_info(VkRenderPass& renderPass, std::vector<VkImageView>& attachments, VkExtent2D& extent, uint32_t layers);

  VkCommandPoolCreateInfo command_pool_create_info(uint32_t queueFamilyIndex);

  VkImageCreateInfo image_create_info(enum VkImageType imageType, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels, uint32_t arrayLayers, VkFormat& format,
    VkImageTiling& tiling, enum VkImageLayout initialLayout, VkImageUsageFlags usage, VkSampleCountFlagBits numSamples, VkImageCreateFlagBits flags,
    enum VkSharingMode shardingMode, uint32_t queueFamilyIndexCount, uint32_t queueFamilyIndices[]);

  VmaAllocationCreateInfo vma_allocation_create_info(enum VmaMemoryUsage usage);

  VkImageMemoryBarrier image_memory_barrier(VkImage& image, VkImageAspectFlags aspectMask, uint32_t baseArrayLayer, 
uint32_t layerCount, uint32_t levelCount, uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED);

  VkImageBlit image_blit(int32_t mipWidth, int32_t mipHeight, VkImageAspectFlags srcAspectMask, VkImageAspectFlags dstAspectMask, uint32_t mipLevel);

  VkImageViewCreateInfo image_view_create_info(VkImage& image, enum VkImageViewType viewType, enum VkFormat format, VkImageAspectFlags aspectFlags,
    uint32_t baseMipLevel, uint32_t mipLevels, uint32_t baseArrayLayer, uint32_t layerCount);

  //VkSamplerCreateInfo sampler_create_info();

  VkSamplerCreateInfo sampler_create_info(enum VkFilter magAndMinFilter, enum VkSamplerAddressMode uvwAddressModes, VkBool32 anisotropyEnable,
    uint32_t maxAnisotropy, enum VkBorderColor borderColour, VkBool32 compareEnable, enum VkCompareOp compareOp, enum VkSamplerMipmapMode mipmapMode,
    float mipLodBias, float minLod, float maxLod);

  VkBufferCreateInfo buffer_create_info(VkDeviceSize& size, VkBufferUsageFlags& usage, enum VkSharingMode sharingMode, uint32_t queueFamilyIndexCount,
    const uint32_t* pQueueFamilyIndices);
  
  VkCommandBufferAllocateInfo command_buffer_allocate_info(enum VkCommandBufferLevel level, VkCommandPool& pool, uint32_t commandBufferCount);

  VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags);

  VkBufferCopy buffer_copy(VkDeviceSize& size);

  VkBufferImageCopy buffer_image_copy(VkDeviceSize byteOffset, uint32_t width, uint32_t height, enum VkImageAspectFlagBits aspectMask, 
    uint32_t mipLevel = 0, uint32_t baseArrayLayer = 0, uint32_t layerCount = 1, uint32_t bufferRowLength = 0, uint32_t bufferImageHeight = 0);

  VkDescriptorPoolSize descriptor_pool_size(enum VkDescriptorType type, uint32_t size);

  VkDescriptorPoolCreateInfo descriptor_pool_create_info(std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets);

  VkRenderPassBeginInfo render_pass_begin_info(VkRenderPass& renderPass, VkFramebuffer& frameBuffer, VkOffset2D offset, VkExtent2D& extent,
    std::vector<VkClearValue>& clearValues);

  VkDescriptorSetAllocateInfo descriptorset_allocate_info(VkDescriptorPool& descriptorPool, const VkDescriptorSetLayout* descriptorSetLayouts);

  VkWriteDescriptorSet write_descriptorset(uint32_t dstBinding, VkDescriptorSet& dstSet, uint32_t descCount, 
    enum VkDescriptorType type, const VkDescriptorBufferInfo* pBufferInfo, const VkDescriptorImageInfo* pImageInfo = nullptr);
}


