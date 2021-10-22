#include "vk_renderer_offscreen.h"
//#include "vk_renderer.h"
#include <iostream>
#include <fstream> 
#include "vk_structures.h"
#include "vk_images.h"
#include <thread>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

Vk::OffscreenRenderer::OffscreenRenderer(GLFWwindow* windowptr, Mediator& mediator)
    : Renderer(windowptr, mediator){}

void Vk::OffscreenRenderer::init(){
    Vk::Renderer::init();
    createOffscreenImageAndView();
    createOffscreenFramebuffer();
    createOffscreenCommandBuffer();
    //createOffscreenImageBuffer();
}

/*void Vk::OffscreenRenderer::createOffscreenImageBuffer(){
    VkBufferCreateInfo createinfo = {};
    createinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createinfo.size = OFFSCREEN_IMAGE_WIDTH * OFFSCREEN_IMAGE_HEIGHT * 4 * sizeof(int8_t);
    createinfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    createinfo.sharingMode = VK_SHARING_MODE_CONCURRENT;

    //create the image copy buffer
    createBuffer(createinfo.size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU, offscreenImageBuffer, offscreenImageBufferAlloc);

    _swapDeletionQueue.push_function([=](){vmaDestroyBuffer(allocator, offscreenImageBuffer, offscreenImageBufferAlloc);});
}*/

void Vk::OffscreenRenderer::createOffscreenCommandBuffer(){
    VkCommandPoolCreateInfo poolInfo = Vk::Structures::command_pool_create_info(queueFamilyIndicesStruct.graphicsFamily.value());
    if(vkCreateCommandPool(device, &poolInfo, nullptr, &offscreenCommandPool) != VK_SUCCESS){
        throw std::runtime_error("Failed to create gui command pool");
    }   
    _swapDeletionQueue.push_function([=](){vkDestroyCommandPool(device, offscreenCommandPool, nullptr);
	});

	//allocate the gui command buffer
	VkCommandBufferAllocateInfo cmdAllocInfo = Vk::Structures::command_buffer_allocate_info(VK_COMMAND_BUFFER_LEVEL_PRIMARY, offscreenCommandPool, 1);
    
	if(vkAllocateCommandBuffers(device, &cmdAllocInfo, &offscreenCommandBuffer) != VK_SUCCESS){
            throw std::runtime_error("Unable to allocate offscreen command buffers");
    }
    _swapDeletionQueue.push_function([=](){vkFreeCommandBuffers(device, offscreenCommandPool, 1, &offscreenCommandBuffer);});
}

void Vk::OffscreenRenderer::createOffscreenFramebuffer(){

    std::vector<VkImageView> attachments = {colourImageView, depthImageView, offscreenImageView};

    VkExtent2D extent{};
    extent.height = OFFSCREEN_IMAGE_HEIGHT;
    extent.width = OFFSCREEN_IMAGE_WIDTH;

    VkFramebufferCreateInfo framebufferInfo = Vk::Structures::framebuffer_create_info(renderPass, attachments, extent, 1);

    if(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &offscreenFramebuffer) != VK_SUCCESS){
        throw std::runtime_error("Failed to create offscreen framebuffer");
    }
    _swapDeletionQueue.push_function([=](){vkDestroyFramebuffer(device, offscreenFramebuffer, nullptr);});
}

void Vk::OffscreenRenderer::createOffscreenImageAndView(){
    imageHelper->createImage(OFFSCREEN_IMAGE_WIDTH, OFFSCREEN_IMAGE_HEIGHT, 1, 1, VK_SAMPLE_COUNT_1_BIT, (VkImageCreateFlagBits)0, swapChainImageFormat, VK_IMAGE_TILING_OPTIMAL, 
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, offscreenImage, offscreenImageAllocation, VMA_MEMORY_USAGE_GPU_ONLY);
    _swapDeletionQueue.push_function([=](){vmaDestroyImage(allocator, offscreenImage, offscreenImageAllocation);});

    offscreenImageView = imageHelper->createImageView(offscreenImage, VK_IMAGE_VIEW_TYPE_2D, swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1);
    _swapDeletionQueue.push_function([=](){vkDestroyImageView(device, offscreenImageView, nullptr);});
}

/*void Vk::Renderer::createOffscreenRenderPass(){
    //in our case we will need a colour buffer represented by one of the images from the swap chain
    VkAttachmentDescription colourAttachment = Vk::Structures::attachment_description(swapChainImageFormat, msaaSamples, VK_ATTACHMENT_LOAD_OP_CLEAR, 
        VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    //Subpasses and attachment references
    //A single rendering pass can consist of multiple subpasses. Subpasses are sunsequent rendering operations that depend on contents of
    //framebuffers in previous passes. Eg a sequence of post processing effects that are applied one after another
    //if you group these operations onto one render pass then Vulkan is able to optimize.
    //For our first triangle we will just have one subpass.
    
    //Every subpass references one or more of the attachments described in the previous sections. These references are VkAttachmentReference structs
    VkAttachmentReference colourAttachmentRef = Vk::Structures::attachment_reference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    //because we need to resolve the above multisampled image before presentation (you cant present a multisampled image because
    //it contains multiple pixel values per pixel) we need an attachment that will resolve the image back to something we can 
    //VK_IMAGE_LAYOUT_PRESENT_SRC_KHR and in standard 1 bit sampling
    //VkAttachmentDescription colourAttachmentResolve = Vk::Structs::attachment_description(swapChainImageFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_DONT_CARE, 
    //    VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    //VkAttachmentDescription colourAttachmentResolve = Vk::Structures::attachment_description(swapChainImageFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_DONT_CARE, 
    //    VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL); //CHANGED TO VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL as it is not the final layout any more, becaus ethe GUI will be drawn in another subpass later

    VkAttachmentDescription colourAttachmentResolve = Vk::Structures::attachment_description(swapChainImageFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_DONT_CARE, 
        VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL); //CHANGED TO VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL as it is not the final layout any more, becaus ethe GUI will be drawn in another subpass later

    //VK_IMAGE_LAYOUT_
    //the render pass now has to be instructed to resolve the multisampled colour image into the regular attachment
    VkAttachmentReference colourAttachmentResolveRef = Vk::Structures::attachment_reference(2, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    //colourAttachmentResolve.finalLayout
    // this index is 2, depth attachment is 1

    // we also want to add a Subpass Dependency as described in drawFrame() to wait for VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT before proceeding 
    //specify indices of dependency and dependent subpasses, 
    //special value VK_SUBPASS_EXTERNAL refers to implicit subpass before or after the the render pass depending on whether specified in srcSubpass/dstSubpass
    //The operations that should waiti on this are in the colour attachment stage and involve the writing of the colour attachment
    //these settings will prevent the transition from happening until its actually necessary and allowed, ie when we want to start writing colours to it
    VkSubpassDependency dependency = Vk::Structures::subpass_dependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    
    //we want to include a depth attachment for depth testing
    // VK_ATTACHMENT_STORE_OP_DONT_CARE; //we dont need to store depth data as it will be discarded every pass
    // VK_IMAGE_LAYOUT_UNDEFINED; //same as colour buffer, we dont care about previous depth contents
    VkAttachmentDescription depthAttachment = Vk::Structures::attachment_description(Vk::Init::findDepthFormat(physicalDevice), msaaSamples, VK_ATTACHMENT_LOAD_OP_CLEAR, 
        VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
 
    VkAttachmentReference depthAttachmentRef = Vk::Structures::attachment_reference(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    // and here is index 1, just described in wrong order

    //we add a reference to the attachment for the first (and only) subpass
    //the index of the attachment in this array is directly referenced from the fragment shader with the layout(location = 0)out vec4 outColor directive.
    //we pass in the colourAttachmentResolveRef struct which will let the render pass define a multisample resolve operation
    //described in the struct, which will let us render the image to screen by resolving it to the right layout
    //pDepthStencilAttachment: Attachment for depth and stencil data        
    VkSubpassDescription subpass = Vk::Structures::subpass_description(VK_PIPELINE_BIND_POINT_GRAPHICS, 1, &colourAttachmentResolveRef, &colourAttachmentRef, &depthAttachmentRef);

    //now we udpate the VkRenderPassCreateInfo struct to refer to both attachments 
    //and the colourAttachmentResolve
    std::vector<VkAttachmentDescription> attachments = {colourAttachment, depthAttachment, colourAttachmentResolve};
    VkRenderPassCreateInfo renderPassInfo = Vk::Structures::renderpass_create_info(attachments, 1, &subpass, 1, &dependency);

    if(vkCreateRenderPass(device, &renderPassInfo, nullptr, &offscreenRenderPass) != VK_SUCCESS){
        throw std::runtime_error("Failed to create render pass");
    }
    _swapDeletionQueue.push_function([=](){vkDestroyRenderPass(device, offscreenRenderPass, nullptr);});
}*/

// Take a screenshot from the current swapchain image
	// This is done using a blit from the swapchain image to a linear image whose memory content is then saved as a ppm image
	// Getting the image date directly from a swapchain image wouldn't work as they're usually stored in an implementation dependent optimal tiling format
	// Note: This requires the swapchain images to be created with the VK_IMAGE_USAGE_TRANSFER_SRC_BIT flag (see VulkanSwapChain::create)
//adapted from Sascha Willems example screenshot example
void Vk::OffscreenRenderer::writeOffscreenImageToDisk(){
    bool supportsBlit = true;

    // Check blit support for source and destination
    VkFormatProperties formatProps;

    // Check if the device supports blitting from optimal images (the swapchain images are in optimal format)
    vkGetPhysicalDeviceFormatProperties(physicalDevice, swapChainImageFormat, &formatProps);
    if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)) {
        std::cerr << "Device does not support blitting from optimal tiled images, using copy instead of blit!" << std::endl;
        supportsBlit = false;
    }

    // Check if the device supports blitting to linear images
    vkGetPhysicalDeviceFormatProperties(physicalDevice, VK_FORMAT_R8G8B8A8_UNORM, &formatProps);
    if (!(formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
        std::cerr << "Device does not support blitting to linear tiled images, using copy instead of blit!" << std::endl;
        supportsBlit = false;
    }

    VkImage srcImage = offscreenImage;

    VkImage dstImage;
    VmaAllocation dstImageAllocation;

    imageHelper->createImage(OFFSCREEN_IMAGE_WIDTH,  OFFSCREEN_IMAGE_HEIGHT, 1, 1, VK_SAMPLE_COUNT_1_BIT, (VkImageCreateFlagBits)0, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_LINEAR, 
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, dstImage, dstImageAllocation, VMA_MEMORY_USAGE_CPU_ONLY);

    VkCommandBufferAllocateInfo cmdBufAllocateInfo{};
    cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufAllocateInfo.commandPool = offscreenCommandPool;
    cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufAllocateInfo.commandBufferCount = 1;

    VkCommandBuffer cmdBuffer;
    vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &cmdBuffer);
    // If requested, also start recording for the new command buffer
    VkCommandBufferBeginInfo cmdBufInfo = Vk::Structures::command_buffer_begin_info(0);
    vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo);

    // Transition source image from present to transfer source layout
    imageHelper->insertImageMemoryBarrier(
        cmdBuffer,
        srcImage,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    // Transition destination image to transfer destination layout
    imageHelper->insertImageMemoryBarrier(
        cmdBuffer,
        dstImage,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    // If source and destination support blit we'll blit as this also does automatic format conversion (e.g. from BGR to RGB)
    if (supportsBlit){
        // Define the region to blit (we will blit the whole swapchain image)
        VkOffset3D blitSize;
        blitSize.x = OFFSCREEN_IMAGE_WIDTH;
        blitSize.y = OFFSCREEN_IMAGE_HEIGHT;
        blitSize.z = 1;
        VkImageBlit imageBlitRegion{};
        imageBlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlitRegion.srcSubresource.layerCount = 1;
        imageBlitRegion.srcOffsets[1] = blitSize;
        imageBlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlitRegion.dstSubresource.layerCount = 1;
        imageBlitRegion.dstOffsets[1] = blitSize;

        // Issue the blit command
        vkCmdBlitImage(
            cmdBuffer,
            srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &imageBlitRegion,
            VK_FILTER_NEAREST);
    }
    else
    {
        // Otherwise use image copy (requires us to manually flip components)
        VkImageCopy imageCopyRegion{};
        imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageCopyRegion.srcSubresource.layerCount = 1;
        imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageCopyRegion.dstSubresource.layerCount = 1;
        imageCopyRegion.extent.width = OFFSCREEN_IMAGE_WIDTH;
        imageCopyRegion.extent.height = OFFSCREEN_IMAGE_HEIGHT;
        imageCopyRegion.extent.depth = 1;

        // Issue the copy command
        vkCmdCopyImage(
            cmdBuffer,
            srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &imageCopyRegion);
    }

    // Transition destination image to general layout, which is the required layout for mapping the image memory later on
    imageHelper->insertImageMemoryBarrier(
        cmdBuffer,
        dstImage,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    flushCommandBuffer(cmdBuffer, graphicsQueue, offscreenCommandPool, false);

    // Get layout of the image (including row pitch)
    VkImageSubresource subResource { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
    VkSubresourceLayout subResourceLayout;
    vkGetImageSubresourceLayout(device, dstImage, &subResource, &subResourceLayout);

    const char* mappedData;
    vmaMapMemory(allocator, dstImageAllocation, (void**)&mappedData);
    mappedData += subResourceLayout.offset;

    // If source is BGR (destination is always RGB) and we can't use blit (which does automatic conversion), we'll have to manually swizzle color components
    bool colorSwizzle = false;
    // Check if source is BGR
    // Note: Not complete, only contains most common and basic BGR surface formats for demonstration purposes

    if (!supportsBlit){
        std::vector<VkFormat> formatsBGR = { VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SNORM };
        colorSwizzle = (std::find(formatsBGR.begin(), formatsBGR.end(), swapChainImageFormat) != formatsBGR.end());
    }

    //std::thread thread(&Vk::Renderer::writeMappedImageToFile, this, std::ref(mappedData), subResourceLayout.rowPitch, colorSwizzle); //run on another thread
    //this should be happening in another thread
    std::ofstream file("filename.ppm", std::ios::out | std::ios::binary);

    // ppm header
    file << "P6\n" << OFFSCREEN_IMAGE_WIDTH << "\n" << OFFSCREEN_IMAGE_HEIGHT << "\n" << 255 << "\n";

    // ppm binary pixel data
    for (uint32_t y = 0; y < OFFSCREEN_IMAGE_HEIGHT; y++){
        unsigned int *row = (unsigned int*)mappedData;
        for (uint32_t x = 0; x < OFFSCREEN_IMAGE_WIDTH; x++){
            if (colorSwizzle){
                file.write((char*)row+2, 1);
                file.write((char*)row+1, 1);
                file.write((char*)row, 1);
            }else
            {
                file.write((char*)row, 3);
            }
            row++;
        }
        mappedData += subResourceLayout.rowPitch;
    }
    file.close();

    std::cout << "Screenshot saved to disk" << std::endl;
    
    vmaUnmapMemory(allocator, dstImageAllocation);
    vkDestroyImage(device, dstImage, nullptr);
}


void Vk::OffscreenRenderer::setShouldDrawOffscreen(bool b){
    //this bool should be atomic, as a timer will trigger from another thread
    shouldDrawOffscreenFrame = b;
}

void Vk::OffscreenRenderer::populateLanderCameraData(GPUCameraData& camData){
    //LanderObj* lander = p_renderables->at(2).get(); //lander is obj 2
    RenderObject* lander =  p_renderables->at(2).get(); //lander is obj 2
    glm::vec3 camPos = lander->pos;
    glm::vec3 landingSitePos = glm::vec3(0,0,0); //origin for now

    //need to rotate by landerForward?
    glm::mat4 view = glm::lookAt(camPos, landingSitePos, lander->up); //fixing the view to landing site
    //glm::mat4 view = glm::lookAt(camPos, camPos + lander->up, lander->forward); //setting view to look forward

    glm::mat4 proj = glm::perspective(glm::radians(OFFSCREEN_IMAGE_FOV), (float) OFFSCREEN_IMAGE_WIDTH / (float) OFFSCREEN_IMAGE_HEIGHT, 0.1f, 15000.0f);
    proj[1][1] *= -1;
    camData.projection = proj;
    camData.view = view;
	camData.viewproj = proj * view;
}

//void Vk::OffscreenRenderer::fillOffscreenImageBuffer(){
 //   imageHelper->copyImageToBuffer(offscreenImageBuffer, offscreenImage, OFFSCREEN_IMAGE_WIDTH, OFFSCREEN_IMAGE_HEIGHT, 1);
    //vkCmdCopyImageToBuffer(offscreenCommandBuffer, offscreenImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, offscreenImageBuffer, 1, &region);

    /*VkImageCopy imageCopyRegion{};
    imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopyRegion.srcSubresource.layerCount = 1;
    imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopyRegion.dstSubresource.layerCount = 1;
    imageCopyRegion.extent.width = OFFSCREEN_IMAGE_WIDTH;
    imageCopyRegion.extent.height = OFFSCREEN_IMAGE_HEIGHT;
    imageCopyRegion.extent.depth = 1;

    vkCmdCopyImage(offscreenCommandBuffer, offscreenImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, offscreenImageB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopyRegion);

    imageHelper->transitionImageLayout(offscreenImageB, swapChainImageFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,  VK_IMAGE_LAYOUT_GENERAL, 1, 1);*/
//}

void Vk::OffscreenRenderer::drawFrame(){
    if (shouldDrawOffscreenFrame){
        shouldDrawOffscreenFrame = false;
        recordCommandBuffer_Offscreen();
    }
    Renderer::drawFrame(); //draw a frame
}

void Vk::OffscreenRenderer::recordCommandBuffer_Offscreen(){
    //similar to recordCommandBuffer_Objects but calculate shadowmap for each light
    vkResetCommandBuffer(offscreenCommandBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT); 

    VkCommandBufferBeginInfo beginInfo = Vk::Structures::command_buffer_begin_info(0);
    if(vkBeginCommandBuffer(offscreenCommandBuffer, &beginInfo) != VK_SUCCESS){
        throw std::runtime_error("Failed to begin recording offscreen command buffer");
    }

    VkExtent2D extent{OFFSCREEN_IMAGE_WIDTH, OFFSCREEN_IMAGE_HEIGHT}; 
    //we need to use our own renderpass here, with offscreen 
    VkRenderPassBeginInfo renderPassBeginInfo = Vk::Structures::render_pass_begin_info(renderPass, offscreenFramebuffer, {0, 0}, extent, clearValues);

    vkCmdBeginRenderPass(offscreenCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    if(r_mediator.application_getSceneLoaded())
    if(p_renderables != NULL && !p_renderables->empty()){   
        VkBuffer vertexBuffers[] = {vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(offscreenCommandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(offscreenCommandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        drawOffscreen(0);
    }
    
    //we we can end the render pass
    vkCmdEndRenderPass(offscreenCommandBuffer);

    //and we have finished recording, we check for errors here
    if (vkEndCommandBuffer(offscreenCommandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record offscreen command buffer!");
    }

    //Submitting the command buffer
    //queue submission and synchronization is configured through VkSubmitInfo struct
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &offscreenCommandBuffer;

    //submit the queue
    queueSubmitMutex.lock();
    if(vkQueueSubmit(graphicsQueue, 1 , &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) { //then fails here after texture flush
        throw std::runtime_error("Failed to submit draw command buffer");
    }

    queueSubmitMutex.unlock();
}

void Vk::OffscreenRenderer::drawOffscreen(int curFrame){
    ////fill a GPU camera data struct
	GPUCameraData camData;
    populateLanderCameraData(camData);
    updateSceneData(camData);
    
    //and copy it to the buffer
	void* data;
	vmaMapMemory(allocator, _frames[curFrame].cameraBufferAllocation, &data);
	memcpy(data, &camData, sizeof(GPUCameraData));
	vmaUnmapMemory(allocator, _frames[curFrame].cameraBufferAllocation);

    //UPDATE TRANSFORM MATRICES OF ALL MODELS HERE
    updateObjectTranslations();

    //fetch latest lighting data
    updateLightingData(camData);
    //map it to the GPU
    mapLightingDataToGPU();
    
    //then do the objects data into the storage buffer
    void* objectData;
    vmaMapMemory(allocator, _frames[curFrame].objectBufferAlloc, &objectData);
    GPUObjectData* objectSSBO = (GPUObjectData*)objectData;
    //loop through all objects in the scene, need a better container than a vector
    for (int i = 0; i < p_renderables->size(); i++){
        objectSSBO[i].modelMatrix = p_renderables->at(i)->transformMatrix;
        //calculate the normal matrix, its done by inverse transpose of the model matrix * view matrix because we are working 
        //in view space in the shader. taking only the top 3x3
        //this is to account for rotation and scaling when using normals. should be done on cpu as its costly
        objectSSBO[i].normalMatrix = glm::transpose(glm::inverse(camData.view * p_renderables->at(i)->transformMatrix)); 
    }
    vmaUnmapMemory(allocator, _frames[curFrame].objectBufferAlloc);
  
	char* sceneData;
    vmaMapMemory(allocator, _sceneParameterBufferAlloc , (void**)&sceneData);
	int frameIndex = frameCounter % MAX_FRAMES_IN_FLIGHT;
	sceneData += pad_uniform_buffer_size(sizeof(GPUSceneData)) * frameIndex; 
	memcpy(sceneData, &_sceneParameters, sizeof(GPUSceneData));
	vmaUnmapMemory(allocator, _sceneParameterBufferAlloc); 

    Material* lastMaterial = nullptr;

    //could have a menu option to disable drawing certain things in offscreen like the asteroid for testing,
    //or in the main renderer pass show things like landing site boxes and other debugging things
    int renderObjectIds [6] = {1,3,4,5,6,7}; //1 = star, 3 = asteroid, 4-7 = landing site boxes
    
    for(const int i : renderObjectIds){
    //for (int i = 1; i < p_renderables->size(); i++){
        RenderObject* object = p_renderables->at(i).get();
        //only bind the pipeline if it doesn't match with the already bound one
        //object->id
		if (object->material != lastMaterial) {
            //object.
            if(object->material == nullptr){throw std::runtime_error("object.material is a null reference in drawObjects()");} //remove if statement in release
			vkCmdBindPipeline(offscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object->material->pipeline);
			lastMaterial = object->material;

            //offset for our scene buffer
            uint32_t scene_uniform_offset = pad_uniform_buffer_size(sizeof(GPUSceneData)) * frameIndex;
            //bind the descriptor set when changing pipeline
            vkCmdBindDescriptorSets(offscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                object->material->pipelineLayout, 0, 1, &_frames[curFrame].globalDescriptor, 1, &scene_uniform_offset);

	        //object data descriptor
        	vkCmdBindDescriptorSets(offscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                object->material->pipelineLayout, 1, 1, &_frames[curFrame].objectDescriptor, 0, nullptr);

             //material descriptor, we are using a dynamic uniform buffer and referencing materials by their offset with propertiesId, which just stores the int relative to the material in _materials
        	vkCmdBindDescriptorSets(offscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                object->material->pipelineLayout, 2, 1, &_frames[curFrame].materialSet, 0, nullptr);
            //the "firstSet" param above is 2 because in init_pipelines its described in VkDescriptorSetLayout[] in index 2!
            
            vkCmdBindDescriptorSets(offscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                object->material->pipelineLayout, 3, 1, &_frames[curFrame].lightSet, 0, nullptr);

            if (object->material->_multiTextureSets.size() > 0) {
                vkCmdBindDescriptorSets(offscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                    object->material->pipelineLayout, 4, 1, &object->material->_multiTextureSets[0], 0, nullptr);
		    }
		}

        //add material property id for this object to push constant
		PushConstants constants;
		constants.matIndex = object->material->propertiesId;
        constants.numPointLights = p_pointLights->size();
        constants.numSpotLights = p_spotLights->size();

        //upload the mesh to the GPU via pushconstants
		vkCmdPushConstants(offscreenCommandBuffer, object->material->pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &constants);

        vkCmdDrawIndexed(offscreenCommandBuffer, _loadedMeshes[object->meshId].indexCount, 1, _loadedMeshes[object->meshId].indexBase, 0, i); //using i as index for storage buffer in shaders
    }
}