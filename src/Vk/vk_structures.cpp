#include <set>
#include "vk_types.h"
#include "vk_structures.h"

//VkSubmitInfo without synchronization, used for immediate transfers
VkSubmitInfo Vk::Structures::submit_info(VkCommandBuffer* commandBuffer){
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = commandBuffer;
	return submitInfo;
}

//VkSubmitInfo with synchronization, used in gpu synching
VkSubmitInfo Vk::Structures::submit_info(VkCommandBuffer commandBuffers[], uint32_t cmdBufferCount, 
		VkSemaphore* waitSemaphores, uint32_t waitCount,
		VkPipelineStageFlags* waitStages,
		VkSemaphore* signalSemaphores, uint32_t signalCount) {
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = waitCount;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.signalSemaphoreCount = signalCount;
	submitInfo.pSignalSemaphores = signalSemaphores;
	submitInfo.commandBufferCount = cmdBufferCount;
	submitInfo.pCommandBuffers = commandBuffers;
	return submitInfo;
}

VkPresentInfoKHR Vk::Structures::present_info(VkSemaphore* signalSemaphores, uint32_t waitCount, 
		VkSwapchainKHR* swapChains, uint32_t *imageIndex){
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = waitCount; //specify which semaphores to wait on before presentation can happen
	presentInfo.pWaitSemaphores = signalSemaphores; //specify which semaphores to wait on before presentation can happen
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains; //specifiy the swap chain to present images to
	presentInfo.pImageIndices = imageIndex; //and the index of the image for each swap chain, wil lalmost always be a single one
	presentInfo.pResults = nullptr; //optional, allows
	return presentInfo;
}

VkApplicationInfo Vk::Structures::app_info(const char* name){
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = name;
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;
	return appInfo;
}

VkInstanceCreateInfo Vk::Structures::instance_create_info(VkApplicationInfo &appInfo, std::vector<const char *> &extensions, const char* const * enabledLayerNames, uint32_t enabledLayerCount, void *next){
	VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = enabledLayerCount;
	createInfo.ppEnabledLayerNames = enabledLayerNames;
    createInfo.pNext = next;
	return createInfo;
}

std::vector<VkDeviceQueueCreateInfo> Vk::Structures::queue_create_infos( std::set<uint32_t> &uniqueQueueFamilies, const float* queuePriority){
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	
	for(uint32_t queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
    }
	return queueCreateInfos;
}

VkDeviceCreateInfo Vk::Structures::device_create_info(VkPhysicalDeviceFeatures& deviceFeatures, std::vector<VkDeviceQueueCreateInfo>& queueCreateInfos, const std::vector<const char*>& deviceExtensions){
	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
	return createInfo;
}

VmaAllocatorCreateInfo Vk::Structures::vma_allocator_info(VkPhysicalDevice& physicalDevice, VkDevice& device, VkInstance& instance, VmaAllocator& allocator){
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    //allocatorInfo.flags
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;
    //allocatorInfo.frameInUseCount = MAX_FRAMES_IN_FLIGHT; //added
    
    vmaCreateAllocator(&allocatorInfo, &allocator);
	return allocatorInfo;
}

VkSwapchainCreateInfoKHR Vk::Structures::swapchain_create_info(VkSurfaceKHR& surface, uint32_t& imageCount, VkSurfaceFormatKHR& surfaceFormat, VkExtent2D& extent, 
		uint32_t queueFamilyIndices[2], VkSurfaceTransformFlagBitsKHR& currentTransform, VkPresentModeKHR& presentMode){
	VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    //after specifiying the surface the swap chain should be tied to, the details of the swap chain iamges are specified
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1; //amount of layers each image consists of, usually one unless its a stereoscopic 3D app
    //specifies with kind of operations we'll use the iamges in the swap chain for
    //for now we are going o render directly to them, this means they are used as color attachment
    //it is also possible to render imaes to a seperate image first to perform operations like post processing
    //in that case you would use a value like VK_IMAGE_USAGE_TRANSFER_DST_BIT instead 
    //and use a memory operation to transfer the rendered image to a swap chain image
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    //next we need to specify how to handle swap chain images that will be used across multiple queue families
    //that will be the case in our app if the graphics queue family is different from the presentation queue.
    //we will be drawing on the image in the swap chain from the graphics queue and then submitting them on the presentation queue.
    //there are two ways to hanfle images that are accessed from multiple queues
    //VK_SHARING_MODE_EXCLUSIVE: image is owned by one queue family at a time and must be explicitly transferred before using it in another queue. best performance
    //VK_SHARING_MODE_CONCURRENT: images can be used across multiple queue families without explicit ownership transfers
    
    if(queueFamilyIndices[0] != queueFamilyIndices[1]){
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT; //can still do exclusive if the families differ. but requires more set up
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else{
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; //will probably be the case on most hardware
    }

    //can specify that a certain transform should be applied to images in the swap chain if it is supported (supportedTransforms in capabilities)
    //like a 90 degree rotation or a horizontal flip. to specify no transformation just set currentTransform
    createInfo.preTransform = currentTransform;
    //compositeAlpha specifies if the alpha channel should be used for blending other windows in the window system, 
    //almost always want to ignore this hence set opaque
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    //set the presentMode we chose earlier
    createInfo.presentMode = presentMode;
    //if clipped is VK_TRUE then we dont care about colour of pixels that are obscured, for example because another window is in front.
    //unless you really need to be able to read these pixels back accurately then you'll get best performance with clipping enabled
    createInfo.clipped = VK_TRUE;
    
    //with Vulkan its possible the swap chain can become invalid or unoptimized while the app is running, eg if the window was resized
    //in that case the swap chain actually needs recreated froms cratch and a reference to the old on specified in this field
    //we will come back to this later, for now lets jsut assume we will only ever create one swap chain
    createInfo.oldSwapchain = VK_NULL_HANDLE;
	return createInfo;
}
VkAttachmentDescription Vk::Structures::attachment_description(VkFormat format, enum VkSampleCountFlagBits samples, enum VkAttachmentLoadOp loadOp, 
        enum VkAttachmentStoreOp storeOp, enum VkImageLayout initialLayout, enum VkImageLayout finalayout){

	VkAttachmentDescription attachmentDesc{};
    attachmentDesc.format = format; //this chould match the format of the swap chain images
    attachmentDesc.samples = samples; //set our max supported msaa value
    //loadOp and storeOp determine waht to do with the data in the attachment before rendering and after rendering. For loadOp we have:
    //VK_ATTACHMENT_LOAD_OP_LOAD: Preserve existing contents of the attachment
    //VK_ATTACHMENT_LOAD_OP_CLEAR: Clear the values to a constant at the start
    //VK_ATTACHMENT_LOAD_OP_DONT_CARE: Existing contents are undefined; we don’t care about them
    attachmentDesc.loadOp = loadOp; //we will clear framebuffer to black before drawing a new frame
    //VK_ATTACHMENT_STORE_OP_STORE: Rendered contents will be stored in memory and can be read later
    //VK_ATTACHMENT_STORE_OP_DONT_CARE: Contents of the framebuffer will be undefined after the rendering operation
    attachmentDesc.storeOp = storeOp; //we want to see the render on screen so we should store the framebuffer
    //loadOp and storeOp apply to colour and depth data. stencilLoadOp and stencilStoreOp apply to stencil data. we dont use stencil so this is irrelevant
    attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    //TExtures and framebuffers in Vukan are represented by VkImage objects with a certain pixel format
    //however the layout of pixels in memory can change based on what you're trying to do with the image. Some of the most common are:
    //K_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: Images used as color attachment
    //VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: Images to be presented in the swapchain
    //VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: Images to be used as destination for a memory copy operation
    attachmentDesc.initialLayout = initialLayout; //we dont care what the previous layout is
    //colourAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; //we want it to be ready for presentation using the swap chain after rendering
    //because we are multisampling we need to use VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, this cant be presented to the screen so we 
    //will need to transition this layout before use further down
    attachmentDesc.finalLayout = finalayout;
    //initialLayout specifies the layout that the iamge will have before the rendering pass
    //finalLayout specifies the layout to transition to after the render pass finishes
    //will be discussed more later, but images need to be transitioned to specific layouts that 
    //are suitable for the operation they they will be involved in next
	return attachmentDesc;
}

VkAttachmentReference Vk::Structures::attachment_reference(uint32_t attachmentIndex, enum VkImageLayout layout){
	VkAttachmentReference attachmentRef{};
    //attachment parameter specifies which attachment to reference by its index in the attachment descriptions array.
    //Our array only has one VkAttachmentDescription so we just need 0 for first element
    attachmentRef.attachment = attachmentIndex;
    //layout specifies which layout we would like the attachment to have during a subpass
    //Vulkan will automatically transition the attachment to this layout when the subpass is started.
    //we intend to use the attachment as a colour buffer and VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL will give us best performance
    attachmentRef.layout = layout;
	return attachmentRef;
}	

VkSubpassDependency Vk::Structures::subpass_dependency(uint32_t srcSubpass, uint32_t dstSubpass,VkPipelineStageFlags srcStageMask, VkAccessFlags srcAccessMask, 
		VkPipelineStageFlags dstStageMask, VkAccessFlags dstAccessMask){
	// we also want to add a Subpass Dependency as described in drawFrame() to wait for VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT before proceeding
    VkSubpassDependency subpassDependency{};
    //specify indices of dependency and dependent subpasses, 
    //special value VK_SUBPASS_EXTERNAL refers to implicit subpass before or after the the render pass depending on whether specified in srcSubpass/dstSubpass
    subpassDependency.srcSubpass = srcSubpass;
    //index 0 refers to our subpass, which is the first and only one, dstSubpass index must be higher than srcSubpass unless one is VK_SUBPASS_EXTERNAL
    subpassDependency.dstSubpass = dstSubpass; 

    //next two fields specify the operations to wait on and the stages in which these ops occur. We need to wait for the swapchain to finish reading from
    //the image before we can access it. This can be accomplished by waiting on the colour attachment output stage itself
    subpassDependency.srcStageMask = srcStageMask;
    subpassDependency.srcAccessMask = srcAccessMask;

    //The operations that should waiti on this are in the colour attachment stage and involve the writing of the colour attachment
    //these settings will prevent the transition from happening until its actually necessary and allowed, ie when we want to start writing colours to it
    subpassDependency.dstStageMask = dstStageMask;;
    subpassDependency.dstAccessMask = dstAccessMask;
	return subpassDependency;
}

VkSubpassDescription Vk::Structures::subpass_description(VkPipelineBindPoint pipelineBindPoint, uint32_t colourAttachmentCount, 
		const VkAttachmentReference* pResolveAttachments, const VkAttachmentReference* pColorAttachments, const VkAttachmentReference* pDepthStencilAttachments){
    VkSubpassDescription subpassDesc{};
    subpassDesc.pipelineBindPoint = pipelineBindPoint;
    subpassDesc.colorAttachmentCount = colourAttachmentCount;  //next we reference the colour attachent
    //the index of the attachment in this array is directly referenced from the fragment shader with the layout(location = 0)out vec4 outColor directive.
    //other types of attachments can be referenced by a subpass
    //pInputAttachments: Attachments that are read from a shader
    //pResolveAttachments: Attachments used for multisampling color attachments
    //we pass in the colourAttachmentResolveRef struct which will let the render pass define a multisample resolve operation
    //described in the struct, which will let us render the image to screen by resolving it to the right layout
    subpassDesc.pResolveAttachments = pResolveAttachments;
    //pDepthStencilAttachment: Attachment for depth and stencil data        
    //pPreserveAttachments: Attachments that are not used by this subpass,but for which the data must be preserved
    subpassDesc.pColorAttachments = pColorAttachments;
    subpassDesc.pDepthStencilAttachment = pDepthStencilAttachments;
	return subpassDesc;
}

VkRenderPassCreateInfo Vk::Structures::renderpass_create_info(std::vector<VkAttachmentDescription>& attachments, uint32_t subpassCount, 
        VkSubpassDescription* pSubpasses,  uint32_t dependencyCount, VkSubpassDependency* pDependencies){
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = pSubpasses;
    renderPassInfo.dependencyCount = dependencyCount; //changed from 1
    renderPassInfo.pDependencies = pDependencies;
    return renderPassInfo;
}

VkDescriptorSetLayoutBinding Vk::Structures::descriptorset_layout_binding(uint32_t binding, enum VkDescriptorType type, uint32_t descriptorCount, VkShaderStageFlags stageFlags, VkSampler* pSampler){
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = binding; //specifies the binding used in the shader (location 0)
    layoutBinding.descriptorType = type; //specified the descriptor type
    //number of values in the array, could be used to specify a transformation for each of the bones in a skeleton for animation for example
    //our MVP transformation is in a single UBO so we're using 1
    layoutBinding.descriptorCount = descriptorCount; 
    //we need to specify in which shader stages the descriptor is going to be referenced
    //The stageFlags field can be a combination of VkShaderStageFlagBits values or the value VK_SHADER_STAGE_ALL_GRAPHICS
    //we jsut need the Vertex shader as thats where it will be used
    layoutBinding.stageFlags = stageFlags;
    layoutBinding.pImmutableSamplers = pSampler; // Optional We will look at this later (only relevant for image sampling related descriptors)
    return layoutBinding;
}

VkDescriptorSetLayoutCreateInfo Vk::Structures::descriptorset_layout_create_info(std::vector<VkDescriptorSetLayoutBinding>& bindings){
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    return layoutInfo;
}

VkPipelineShaderStageCreateInfo Vk::Structures::pipeline_shader_stage_create_info(enum VkShaderStageFlagBits pipelineStage, VkShaderModule &shaderModule, const VkSpecializationInfo* pSpecialInfo){
    //to actually use the shaders we will  need to assign them to a specific pipeline stage 
    //through VkPipelineShaderStageCreateInfo struct as part of the pipeline creation process
    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = pipelineStage; //specifies in which pipeline stage the shader will be used, this is vertex
    shaderStageInfo.module = shaderModule; //specifies the shader module
    shaderStageInfo.pName = "main"; //specifies the entry point
    shaderStageInfo.pSpecializationInfo = pSpecialInfo; //optional, can be used to specify constants to be used that are compiled at runtime
    return shaderStageInfo;
}

//VkPipelineVertexInputStateCreateInfo Vk::Structs::pipeline_vertex_input_create_info(uint32_t bindingDescriptionCount, const VkVertexInputBindingDescription* pBindingDescriptions, 
//    const VkVertexInputAttributeDescription* pVertexAttributeDescriptions,  uint32_t vertexAttributeDescriptionCount){
//VkPipelineVertexInputStateCreateInfo Vk::Structs::pipeline_vertex_input_create_info(uint32_t bindingDescriptionCount, const VkVertexInputBindingDescription* pBindingDescriptions, 
//       std::vector<VkVertexInputAttributeDescription>& attributeDescriptions){
VkPipelineVertexInputStateCreateInfo Vk::Structures::pipeline_vertex_input_create_info(VertexInputDescription& vertexInputDescription){
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputDescription.bindings.size()); // there is only one binding description
    vertexInputInfo.pVertexBindingDescriptions = vertexInputDescription.bindings.data(); //pass the reference to binding description
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputDescription.attributes.size()); //multiple (2) attribute descriptions (colour and pos)
    vertexInputInfo.pVertexAttributeDescriptions = vertexInputDescription.attributes.data(); //pass data from the attributes description
    //we will also need a Vertex Buffer to be created, we will call this in initVulkan()
    return vertexInputInfo;
}

VkPipelineInputAssemblyStateCreateInfo Vk::Structures::pipeline_input_assembly_state_create_info(enum VkPrimitiveTopology topology, VkBool32 restartEnabled){
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = topology;
    inputAssembly.primitiveRestartEnable = restartEnabled;
    return inputAssembly;
}

VkViewport Vk::Structures::viewport(float x, float y, float width, float height, float minDepth, float maxDepth){
    VkViewport viewport{};
    viewport.x = x;
    viewport.y = y;
    viewport.width = width;
    viewport.height = height;
    viewport.minDepth = minDepth;
    viewport.maxDepth = maxDepth;
    return viewport;
}

VkRect2D Vk::Structures::scissor(int32_t offsetX, int32_t offsetY, VkExtent2D& extent){
    VkRect2D scissor{};
    scissor.offset = {offsetX, offsetY};
    scissor.extent = extent;
    return scissor;
}

VkPipelineViewportStateCreateInfo Vk::Structures::pipeline_viewport_state_create_info(uint32_t viewportCount, const VkViewport* viewport, uint32_t scissorCount, const VkRect2D* scissor){
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = scissor;
    return viewportState;
}

VkPipelineRasterizationStateCreateInfo Vk::Structures::pipeline_rasterization_state_create_info(enum VkPolygonMode polygonMode, VkCullModeFlags cullMode,
       enum VkFrontFace frontFace, VkBool32 depthClamp, VkBool32 depthBiasEnable, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor, VkBool32 discard){
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;;
    rasterizer.depthClampEnable = depthClamp; //if set to true then fragments beyond near and far plane are clamped to them, can be useful for shadow maps
    rasterizer.rasterizerDiscardEnable = discard; //if true then geometry is never passed through this stage, no output to framebuffer
    //polygonMode determines how fragments are generated for Geometry
    //VK_POLYGON_MODE_FILL: fill the area of the polygon with fragments
    //VK_POLYGON_MODE_LINE: polygon edges are drawn as lines
    //VK_POLYGON_MODE_POINT: polygon vertices drawn as points
    rasterizer.polygonMode = polygonMode; //any mode other than fill requires enabling a GPU feature
    rasterizer.lineWidth = 1.0f; //thickness in terms of number of fragments, maximum width depends on hardware. > 1f requires wideLines GPU feature enabled
    rasterizer.cullMode = cullMode; //disable, cull front face, cull back face or cull both
    //rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE; //vertex order to determine front face
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; //vertex order to determine front face
    rasterizer.depthBiasEnable = depthBiasEnable; //sometimes used for shaddow mapping
    rasterizer.depthBiasConstantFactor = depthBiasConstantFactor; //optional
    rasterizer.depthBiasClamp = depthBiasClamp; //optional
    rasterizer.depthBiasSlopeFactor = depthBiasSlopeFactor; //optional
    return rasterizer;
}

VkPipelineMultisampleStateCreateInfo Vk::Structures::pipeline_msaa_state_create_info(VkSampleCountFlagBits rasterizationSamples){
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = rasterizationSamples;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.minSampleShading = 1.0f; //optional
    multisampling.pSampleMask = nullptr; // optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // optional
    multisampling.alphaToOneEnable = VK_FALSE; // otional
    return multisampling;
}

VkPipelineDepthStencilStateCreateInfo Vk::Structures::pipeline_depth_stencil_state_create_info(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, enum VkCompareOp depthCompareOp,
        VkBool32 depthBoundsTestEnable, float minDepthBound, float maxDepthBound){
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    //specifies if the depth of new fragments should be compared to the depth buffer to se eif they should be discarded
    depthStencil.depthTestEnable = depthTestEnable;
    //specifies if new depth fragments should be written to the depth buffer, useful for transparent objects,
    //they should be comapred to previously rendered opaque objects, but not cause further away transparent objects to be discarded
    depthStencil.depthWriteEnable = depthWriteEnable;
    //specifies the comparison to perform, we are sticking to the conventional lower depth = closer, so depth of new fragments should be less
    depthStencil.depthCompareOp = depthCompareOp;
    //use for optional depth bound testing, allows you to keep fragments that fall in a specified depth range, we keep dont need it
    depthStencil.depthBoundsTestEnable = depthBoundsTestEnable;
    depthStencil.minDepthBounds = minDepthBound; // Optional
    depthStencil.maxDepthBounds = maxDepthBound; // Optional
    //we wont be using this
    depthStencil.stencilTestEnable = VK_FALSE;
    return depthStencil;    
}

VkPipelineColorBlendAttachmentState Vk::Structures::pipeline_colour_blend_attachment(VkColorComponentFlags colourComponentFlags, VkBool32 blendEnable, enum VkBlendFactor srcCBlendFactor, enum VkBlendFactor dstCBlendFactor, 
        enum VkBlendFactor srcABlendFactor, enum VkBlendFactor dstABlendFactor){
    VkPipelineColorBlendAttachmentState colourBlendAttachment{};
    colourBlendAttachment.colorWriteMask = colourComponentFlags;
    colourBlendAttachment.blendEnable = blendEnable;
    colourBlendAttachment.srcColorBlendFactor = srcCBlendFactor; // optional
    colourBlendAttachment.dstColorBlendFactor = dstCBlendFactor; // optional
    colourBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // optional
    colourBlendAttachment.srcAlphaBlendFactor = srcABlendFactor; // optional
    colourBlendAttachment.dstAlphaBlendFactor = dstABlendFactor; // optional
    colourBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; //optional
    return colourBlendAttachment;
    /*The most common way to use color blending is to implement alpha blending,where we want the new color to be blended 
    //with the old color based on its opacity. The finalColor should then be computed as follows:
    //finalColor.rgb = newAlpha * newColor + (1 - newAlpha) * oldColor;
    //finalColor.a = newAlpha.a;
    //This can be accomplished with the following parameters:
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    //All possible operations in VkBlendFactor and VkBlendOp in enumerations in the specification*/
}

VkPipelineColorBlendStateCreateInfo Vk::Structures::pipeline_colour_blend_state_create_info(uint32_t attachmentCount, const VkPipelineColorBlendAttachmentState* colourBlendAttachment){
    VkPipelineColorBlendStateCreateInfo colourBlending{};
    colourBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colourBlending.logicOpEnable = VK_FALSE;
    colourBlending.logicOp = VK_LOGIC_OP_COPY; //optional
    colourBlending.attachmentCount = attachmentCount;
    colourBlending.pAttachments = colourBlendAttachment;
    colourBlending.blendConstants[0] = 0.0f; //optional
    colourBlending.blendConstants[1] = 0.0f; //optional
    colourBlending.blendConstants[2] = 0.0f; //optional
    colourBlending.blendConstants[3] = 0.0f; //optional
    return colourBlending;
}
VkPipelineDynamicStateCreateInfo Vk::Structures::pipeline_dynamic_state_create_info(uint32_t dynamicStateCount, const VkDynamicState* dynamicStates){
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = dynamicStateCount;
    dynamicState.pDynamicStates = dynamicStates;
    return dynamicState;
}

VkPipelineLayoutCreateInfo Vk::Structures::pipeline_layout_create_info(uint32_t setLayoutCount, const VkDescriptorSetLayout* pSetLayouts, 
        uint32_t pushConstRangeCount, const VkPushConstantRange* pushConstantRanges){
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = setLayoutCount; //optional
    pipelineLayoutInfo.pSetLayouts = pSetLayouts; //we reference our descriptor set here so the pipeline layout is aware of our set layout
    pipelineLayoutInfo.pushConstantRangeCount = pushConstRangeCount; //optional
    pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges; //optional
    return pipelineLayoutInfo;
}

VkGraphicsPipelineCreateInfo Vk::Structures::graphics_pipeline_create_info(uint32_t stageCount,const std::vector<VkPipelineShaderStageCreateInfo>& shaderStages, 
        const VkPipelineVertexInputStateCreateInfo* vertexInputInfo, const VkPipelineInputAssemblyStateCreateInfo* inputAssembly,
        const VkPipelineViewportStateCreateInfo* viewportState, const VkPipelineRasterizationStateCreateInfo* rasterizer,
        const VkPipelineMultisampleStateCreateInfo* multisampling, const VkPipelineDepthStencilStateCreateInfo* depthStencil,
        const VkPipelineColorBlendStateCreateInfo* colourBlending, const VkPipelineLayout& pipelineLayout, VkRenderPass& renderPass, uint32_t subpassIndex){
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = stageCount;
    pipelineInfo.pStages = shaderStages.data();

    //then we reference all the structures describing the fixed function stage
    pipelineInfo.pVertexInputState = vertexInputInfo;
    pipelineInfo.pInputAssemblyState = inputAssembly;
    pipelineInfo.pViewportState = viewportState;
    pipelineInfo.pRasterizationState = rasterizer;
    pipelineInfo.pMultisampleState = multisampling;
    pipelineInfo.pDepthStencilState = depthStencil; //include our depth stencil that we specified above
    pipelineInfo.pColorBlendState = colourBlending;
    pipelineInfo.pDynamicState = nullptr; //optional

    //then the pipeline layout which is a Vulkan handle rather than a struct pointer
    pipelineInfo.layout = pipelineLayout;

    //finally the reference to the render pass and the index of the pass where the graphics pipeline will be used
    //it is possible to use other render with this pipeline instead of this specific instance, but they have to be compatible with renderPass.
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = subpassIndex;

    //can derive from a base pipeile which allows us to create a new graphics pipeline by deriving from an existing one. the idea being that its less
    //expensive to set up a new pipeline when a lot of the functionality overlaps and switching from the parent can be done quicker.
    //you can specify the handle of an existing pipeline or the index (only used if you specify VK_PIPELINE_CREATE_DERIVATIVE_BIT in VkGraphicsPipelineCreateInfo)
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional
    return pipelineInfo;
}

VkShaderModuleCreateInfo Vk::Structures::shader_module_create_info(const std::vector<char>& code){
VkShaderModuleCreateInfo shaderCreateInfo{};
    shaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderCreateInfo.codeSize = code.size();
    //because the size of the bytecode is specified in bytes, but the bytecode pointer is a uint32_t pointer rather cthan char pointer
    //we have to reinterpret_cast, when casting like this we also need to make sure the data satisfies the alignment requirements of uint32_t
    //luckily vector already ensures the data satisfies the worst case alignment requirements
    shaderCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    return shaderCreateInfo;
}

VkFramebufferCreateInfo Vk::Structures::framebuffer_create_info(VkRenderPass& renderPass, std::vector<VkImageView>& attachments, VkExtent2D& extent, uint32_t layers){
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass; //specify which renderPass the framebuffer uses, needs to be campatible, basically the same number and attachments
    framebufferInfo.attachmentCount = attachments.size();
    framebufferInfo.pAttachments = attachments.data(); //count and pAttachments specify the VkImageView objects that should be bound to the attachment in the pAttachment array
    framebufferInfo.width = extent.width;
    framebufferInfo.height = extent.height;
    framebufferInfo.layers = layers; //our swap chain images are single iamges so layers is 1
    return framebufferInfo;
}

VkCommandPoolCreateInfo Vk::Structures::command_pool_create_info(uint32_t queueFamilyIndex){
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndex; //we are submitting drawing commands so we need graphics queue family
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; //optional VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    return poolInfo;
}

VkImageCreateInfo Vk::Structures::image_create_info(enum VkImageType imageType, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels, uint32_t arrayLayers, VkFormat& format,
        VkImageTiling& tiling, enum VkImageLayout initialLayout, VkImageUsageFlags usage, VkSampleCountFlagBits numSamples, VkImageCreateFlagBits flags,
        enum VkSharingMode shardingMode, uint32_t queueFamilyIndexCount, uint32_t queueFamilyIndices[]){
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    //image type can be 1D 2D 3D, 1D can be used to store an array of data or gradient, 2D mainly textures, 3D for voxel volumes
    imageInfo.imageType = imageType;
    //extent fiel specifies the dimension of the image, basiclly how many texels on each axis, which is why depth must be 1 and not 0
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = depth; 
    imageInfo.mipLevels = mipLevels; //specify our mip levels number we calculated earlier
    imageInfo.arrayLayers = arrayLayers; //no array
    //we should use the same format for the texels as the pixels in the buffer, otherwise copy op will fail
    imageInfo.format = format;
    //tiling field can be:
    //VK_IMAGE_TILING_LINEAR: Texels are laid out in row-major order like our pixels array
    //VK_IMAGE_TILING_OPTIMAL: Texels are laid out in an implementation defined order for optimal access
    //Unlike layout of an image, tiling mode cannot be changed at a later time, If we want to directly access texels in memory we
    //have to use tiling_linear. We will be using a staging buffer instead of a staging image so this wont be necessary and we can
    //use VK_IMAGE_TILING_OPTIMAL for efficient access from the shader
    imageInfo.tiling = tiling;
    //only two possible initial layouts of an image
    //VK_IMAGE_LAYOUT_UNDEFINED: Not usable by the GPU and the very first transition will discard the texels.
    //VK_IMAGE_LAYOUT_PREINITIALIZED: Not usable by the GPU, but the first transition will preserve the texels.
    //there are a few situations where it is necessary for texels to be preserved during the first transision, eg if you wanted to use an image
    //as a staging image in combination with the VK_IMAGE_TILING_LINEAR layout. In that case you'd want to uploda the texel data to it and then
    //transition the image to be a transfer source without losing the data
    //In our case though we will first transition the image to be a transfer destination and then copy texel data to it from a buffer object
    //so we dont need this property and can safely use VK_IMAGE_LAYOUT_UNDEFINED
    imageInfo.initialLayout = initialLayout;

    //same semantics as during buffer creation
    //we will make this transfer destination. We also want to be able to access the iamge from the shader to colour our mesh, so the usage
    //should include VK_IMAGE_USAGE_SAMPLED_BIT
    imageInfo.usage = usage;

    imageInfo.samples = numSamples; //only relevant to multisampling, images used as attachments, we can skip for now
    // Optional flag for images that are related to Sparse Images, images where only certain regions are backed with memory
    //for example if using a 3D texture for a voxel terrain, you could use this to avoid writting all the "Air" values to memory
    imageInfo.flags = flags; 

    //if only one queue family accesses it it should be VK_SHARING_MODE_EXCLUSIVE;
    //but we are transferring on the transient queue to we are using VK_SHARING_MODE_CONCURRENT, probably not ideal
    imageInfo.sharingMode = shardingMode;
    imageInfo.queueFamilyIndexCount = queueFamilyIndexCount;
    imageInfo.pQueueFamilyIndices = queueFamilyIndices;
    return imageInfo;
}

VmaAllocationCreateInfo Vk::Structures::vma_allocation_create_info(enum VmaMemoryUsage usage){
    VmaAllocationCreateInfo allocInfo{};
    //VMA_MEMORY_USAGE_GPU_ONLY;
    //VMA_MEMORY_USAGE_CPU_ONLY;
    //allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    //allocInfo.preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    allocInfo.usage = usage;
    return allocInfo;
}

VkImageMemoryBarrier Vk::Structures::image_memory_barrier(VkImage& image, VkImageAspectFlags aspectMask, uint32_t baseArrayLayer, 
uint32_t layerCount, uint32_t levelCount, uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex){
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    //if you are using the barrier to transfer queue family ownership then these two fields should be the indices of the queue families
    //they msut be set to VK_QUEUE_FAMILY_IGNORED if you dont want to use these
    barrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
    barrier.dstQueueFamilyIndex = dstQueueFamilyIndex;
    barrier.subresourceRange.aspectMask = aspectMask;
    barrier.subresourceRange.baseArrayLayer = baseArrayLayer;
    barrier.subresourceRange.layerCount = layerCount;
    barrier.subresourceRange.levelCount = levelCount;
    return barrier;
}

VkImageBlit Vk::Structures::image_blit(int32_t mipWidth, int32_t mipHeight, VkImageAspectFlags srcAspectMask, VkImageAspectFlags dstAspectMask, uint32_t mipLevel){
    VkImageBlit blit{};
    blit.srcOffsets[0] = { 0, 0, 0 };
    blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
    blit.srcSubresource.aspectMask = srcAspectMask;
    blit.srcSubresource.mipLevel = mipLevel - 1;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.dstOffsets[0] = { 0, 0, 0 };
    blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
    blit.dstSubresource.aspectMask = dstAspectMask;
    blit.dstSubresource.mipLevel = mipLevel;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;
    return blit;
}

VkImageViewCreateInfo Vk::Structures::image_view_create_info(VkImage& image, enum VkImageViewType viewType, enum VkFormat format, VkImageAspectFlags aspectFlags,
        uint32_t baseMipLevel, uint32_t mipLevels, uint32_t baseArrayLayer, uint32_t layerCount){
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = viewType;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = baseMipLevel;
    viewInfo.subresourceRange.levelCount = mipLevels; //specify our mip level count here
    viewInfo.subresourceRange.baseArrayLayer = baseArrayLayer;
    viewInfo.subresourceRange.layerCount = layerCount;
    return viewInfo;
}

VkSamplerCreateInfo Vk::Structures::sampler_create_info(enum VkFilter magAndMinFilter, enum VkSamplerAddressMode uvwAddressModes, VkBool32 anisotropyEnable,
        uint32_t maxAnisotropy, enum VkBorderColor borderColour, VkBool32 compareEnable, enum VkCompareOp compareOp, enum VkSamplerMipmapMode mipmapMode,
        float mipLodBias, float minLod, float maxLod){
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    //mag and min specify how to interpolate texels that are magnified or minified
    //magnification concerns the oversampling problem described above, minification concerns undersampling
    samplerInfo.magFilter = magAndMinFilter; //VK_FILTER_NEAREST or linear, linear apply filtering,
    samplerInfo.minFilter = magAndMinFilter;

    //address mode can be specified per axis
    //note that axes are listed as U,V,W instead of X,Y,Z. This is a convention for texture space coordinates
    samplerInfo.addressModeU = uvwAddressModes;
    samplerInfo.addressModeV = uvwAddressModes;
    samplerInfo.addressModeW = uvwAddressModes;

    //these two fields specifiy if anisotropic filtering should be used. No reason not to unless performance is a concern
    samplerInfo.anisotropyEnable = anisotropyEnable;
    //limits the amount of texel samples to use to calculate the colour, lower is quicker, no gpus go higher than 16 because its negligable difference
    samplerInfo.maxAnisotropy = maxAnisotropy; 

    //which colour is returned beyond the image with clamp to border addressing mode (white black or transparent)
    samplerInfo.borderColor = borderColour;

    //specifies which coordinate system to use when addressing texels in the image
    //if true you address [0,texWidth] [0, texHeight] if false [0,1] range on all axis, best to leave off
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    //if this is enabled texels will first be compared to a value and the result of the comparison is used in filtering operations
    //mainly used for percentage-closer shadow maps! we will come back to this later on
    samplerInfo.compareEnable = compareEnable;
    samplerInfo.compareOp = compareOp;

    //all these refer to mipmapping
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = mipLodBias;
    samplerInfo.minLod = minLod; //4.0f
    samplerInfo.maxLod = maxLod; //max LOD for mipmapped texture 
    return samplerInfo;
}

VkBufferCreateInfo Vk::Structures::buffer_create_info(VkDeviceSize& size, VkBufferUsageFlags& usage, enum VkSharingMode sharingMode, uint32_t queueFamilyIndexCount,
    const uint32_t* pQueueFamilyIndices){
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    //just like images in the swap chain, buffers can be owned by specific queue family or shared between multiple at the same time
    //bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; //we can stick to exclusive as it will only be used from the gaphics queue
    //we are creating a transfer only buffer so we will use concurrent as the transfer queue and graphics queue will need access ? 
    bufferInfo.sharingMode = sharingMode; 
    //If sharingMode is SHARING_MODE_CONCURRENT, queueFamilyIndexCount must be greater than 1
    bufferInfo.queueFamilyIndexCount = queueFamilyIndexCount; //2 because this will be accessed by first the transfer queue family and then the vertex queue family
    //If sharingMode is SHARING_MODE_CONCURRENT, pQueueFamilyIndices must be a valid pointer to an array of queueFamilyIndexCount uint32_t values
    //now we specify the graphics and transfer queue families
    //QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    bufferInfo.pQueueFamilyIndices = pQueueFamilyIndices;
    return bufferInfo;
}

VkCommandBufferAllocateInfo Vk::Structures::command_buffer_allocate_info(enum VkCommandBufferLevel level, VkCommandPool& pool, uint32_t commandBufferCount){
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = level;
    allocInfo.commandPool = pool; //these are single use commands so we assume they are transient by nature
    allocInfo.commandBufferCount = commandBufferCount;
    return allocInfo;
}

VkCommandBufferBeginInfo Vk::Structures::command_buffer_begin_info(VkCommandBufferUsageFlags flags){
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT ; //good practice to signal this is a one time submission.
    return beginInfo;
}

VkBufferCopy Vk::Structures::buffer_copy(VkDeviceSize& size){
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0; //optional
    copyRegion.dstOffset = 0; //optional
    copyRegion.size = size;
    return copyRegion;
}

VkBufferImageCopy Vk::Structures::buffer_image_copy(VkDeviceSize byteOffset, uint32_t width, uint32_t height, enum VkImageAspectFlagBits aspectMask, 
    uint32_t mipLevel, uint32_t baseArrayLayer, uint32_t layerCount, uint32_t bufferRowLength, uint32_t bufferImageHeight){
    VkBufferImageCopy region{};
    region.bufferOffset = byteOffset; //byte offset in buffer at which pixel values start
    //specifies how pixels are laid out in memory, eg you could have some padding bytes between rows of the image, 0 for both means the pixels are
    //simply tightly packed like they are in our case. 
    //imageSubresource, imageOffset and imageExtent fields indicate to which part of the image we want to copy the pixels.
    region.bufferRowLength = bufferRowLength; 
    region.bufferImageHeight = bufferImageHeight;
    region.imageSubresource.aspectMask = aspectMask;
    region.imageSubresource.mipLevel = mipLevel;
    region.imageSubresource.baseArrayLayer = baseArrayLayer;
    region.imageSubresource.layerCount = layerCount;
    region.imageOffset = {0,0,0};
    region.imageExtent = {width, height, 1};
    return region;
}

VkDescriptorPoolSize Vk::Structures::descriptor_pool_size(enum VkDescriptorType type, uint32_t size){
    VkDescriptorPoolSize poolSize;
    poolSize.type = type;
    poolSize.descriptorCount = size;
    return poolSize;
}

VkDescriptorPoolCreateInfo Vk::Structures::descriptor_pool_create_info(std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets){
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    //aside from the max number of individual descriptors that are available, we also need to specify the max number descriptor sets that may be allocated
    poolInfo.maxSets = maxSets;
    //an optional flag similar to command pools determines if individual descriptor sets can be freed or now VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
    return poolInfo;
}

VkRenderPassBeginInfo Vk::Structures::render_pass_begin_info(VkRenderPass& renderPass, VkFramebuffer& frameBuffer, VkOffset2D offset, VkExtent2D& extent,
        std::vector<VkClearValue>& clearValues){
    //starting a render pass
    VkRenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = renderPass;
    renderPassBeginInfo.framebuffer = frameBuffer; //we created a framebuffer for each swap chain image that specifies it as colour attachment
    //then we define the render area, defines where shader loads and stores take place, should match size of the attachments for performance
    renderPassBeginInfo.renderArea.offset = offset;
    renderPassBeginInfo.renderArea.extent = extent;
    //define the clear value for VK_ATTACHMENT_LOAD_OP_CLEAR, which is used as a load operation for colour attachment
    //VkClearValue clearColour = {0.0f, 0.0f, 0.0f, 1.0f}; //black with 100% opacity
    //because we have multiple attachments with VK_ATTACHMENT_LOAD_OP_CLEAR, we also need to specify multiple clear values

    renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassBeginInfo.pClearValues = clearValues.data();
    //note that the order of clear values should be identical to the order of attachments
    return renderPassBeginInfo;
}

VkDescriptorSetAllocateInfo Vk::Structures::descriptorset_allocate_info(VkDescriptorPool& descriptorPool, const VkDescriptorSetLayout* descriptorSetLayouts){
  VkDescriptorSetAllocateInfo allocInfo ={};
    allocInfo.pNext = nullptr;
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = descriptorSetLayouts; 
    return allocInfo;
}

VkWriteDescriptorSet Vk::Structures::write_descriptorset(uint32_t dstBinding, VkDescriptorSet& dstSet, uint32_t descCount, enum VkDescriptorType type, 
        const VkDescriptorBufferInfo* pBufferInfo, const VkDescriptorImageInfo* pImageInfo){
    VkWriteDescriptorSet setWrite{};
    setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    setWrite.pNext = nullptr;
    //we are going to write into binding number 0
    setWrite.dstBinding = dstBinding;
    //of the global descriptor
    setWrite.dstSet = dstSet;
    setWrite.descriptorCount = descCount;
    //and the type is uniform buffer
    setWrite.descriptorType = type;
    setWrite.pBufferInfo = pBufferInfo;
    setWrite.pImageInfo = pImageInfo;
    return setWrite;
}