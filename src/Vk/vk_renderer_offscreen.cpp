#include "vk_renderer_offscreen.h"
#include <iostream>
#include <fstream> 
#include "vk_structures.h"
#include "vk_images.h"
#include <thread>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include "vk_init_queries.h"
#include "vk_pipeline.h"

//Offscreen rendering set up, used to simulate the lander optical camera
//derived from vk_renderer, optionally performs offscreen rending, and runs renderer class

Vk::OffscreenRenderer::OffscreenRenderer(GLFWwindow* windowptr, Mediator& mediator)
    : Renderer(windowptr, mediator){}

void Vk::OffscreenRenderer::init(){
    Vk::Renderer::init();
    
    //resize structures holding images, pointers to mapped memory, indices for ordering and availability... complicated
    //basically coordinates writing/reading from offscreen renderer, lander opencv component, and ImGUI
    opticsTextures.resize(NUM_TEXTURE_SETS);
    detectionTextures.resize(NUM_TEXTURE_SETS);

    imguiTextureSetIndicesQueue.resize(0);
    imguiDetectionIndicesQueue.resize(0);
    imguiMatchIndicesQueue.resize(0);
    cvMatQueue.resize(0);
   
    imguiTexturePackets.resize(NUM_TEXTURE_SETS*NUM_TEXTURES_IN_SET+1); //+1 for the match image
    detectionImageMappings.resize(NUM_TEXTURE_SETS);

    createOffscreenColourResources();
    createOffscreenDepthResources();
    createSamplers();
    createOffscreenImageAndView();
    createOffscreenImageBuffer();
    createOffscreenCameraBuffer();
    createOffscreenDescriptorSet();
    createOffscreenRenderPass();
    createOffscreenFramebuffer();
    createOffscreenCommandBuffer();
    createOffscreenSyncObjects();

    if(Service::OUTPUT_TEXT){
        //output world light details of the optics camera
        r_mediator.writer_writeToFile("PARAMS", "LightAmbient:" + glm::to_string(LANDER_OPTICS_AMBIENT));
        r_mediator.writer_writeToFile("PARAMS", "LightDiffuse:" + glm::to_string(LANDER_OPTICS_DIFFUSE));
        r_mediator.writer_writeToFile("PARAMS", "LightSpecular:" + glm::to_string(LANDER_OPTICS_SPECULAR));
    }
}

//this sampler will be used by the patched version of ImGUI to draw the image
void Vk::OffscreenRenderer::createSamplers(){
    VkSamplerCreateInfo dstSamplerInfo = Vk::Structures::sampler_create_info(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE, 16, 
        VK_BORDER_COLOR_INT_OPAQUE_BLACK, VK_FALSE, VK_COMPARE_OP_ALWAYS, VK_SAMPLER_MIPMAP_MODE_LINEAR, 0.0f, 0.0f, 1); //just 1 for now, 
    vkCreateSampler(device, &dstSamplerInfo, nullptr, &greyRGBImageSampler);
    _mainDeletionQueue.push_function([=](){vkDestroySampler(device, greyRGBImageSampler, nullptr);});

}

//fences are used to synchronise cpu with gpu operations,
//semaphores are used to sync gpu operations with other gpu operations
//so here we need fences to sync rendering offscreen images with cpu writing to file
void Vk::OffscreenRenderer::createOffscreenSyncObjects(){
    //offscreenCopyFence will signal once rendering is complete and we can copy to file
    //additionally rendering wont start unless offscreenCopyFence is signalled complete by the previous render pass
    VkFenceCreateInfo uploadFenceCreateInfo{};
    uploadFenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    uploadFenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; //with this fence we wont set signal complete because we dont want to wait on it before submit
    if(vkCreateFence(device, &uploadFenceCreateInfo, nullptr, &offscreenCopyFence) != VK_SUCCESS){
        throw std::runtime_error("Failed to create upload Fence");
    }		
    _mainDeletionQueue.push_function([=](){vkDestroyFence(device, offscreenCopyFence, nullptr);});
}

void Vk::OffscreenRenderer::createOffscreenCameraBuffer(){
    //create a uniform buffer to hold lander camera buffer information
    createBuffer(sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, 
        offscreenCameraBuffer, offscreenCameraBufferAllocation);
    _swapDeletionQueue.push_function([=](){vmaDestroyBuffer(allocator, offscreenCameraBuffer, offscreenCameraBufferAllocation);});

    createBuffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, 
        offscreenSceneParameterBuffer, offscreenSceneParameterBufferAlloc);
    _swapDeletionQueue.push_function([=](){vmaDestroyBuffer(allocator, offscreenSceneParameterBuffer, offscreenSceneParameterBufferAlloc);});

    createBuffer(sizeof(GPUSpotLightData) * MAX_LIGHTS, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, 
        os_spotLightParameterBuffer, os_spotLightParameterBufferAlloc);
    _swapDeletionQueue.push_function([=](){vmaDestroyBuffer(allocator, os_spotLightParameterBuffer, os_spotLightParameterBufferAlloc);});

    createBuffer(sizeof(GPUObjectData) * MAX_OBJECTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, 
        os_objectBuffer, os_objectBufferAlloc);
    _swapDeletionQueue.push_function([=](){vmaDestroyBuffer(allocator, os_objectBuffer, os_objectBufferAlloc);});
}

//to get around sharing issues we use a different descripter set, substituting globalset of the base renderer with offscreenDescriptorSet that has its own camBufferBinding
void Vk::OffscreenRenderer::createOffscreenDescriptorSet(){
    //Create Descriptor Set layout describing the _globalSetLayout containing global scene and camera data
    //Cam is set 0 binding 0 because its first in this binding set
    //Scene is set 0 binding 1 because its second in this binding set
    VkDescriptorSetLayoutBinding camBufferBinding = Vk::Structures::descriptorset_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr);
    //binding for scene data at 1 in vertex and frag
	VkDescriptorSetLayoutBinding sceneBind = Vk::Structures::descriptorset_layout_binding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    std::vector<VkDescriptorSetLayoutBinding> bindings =  {camBufferBinding, sceneBind};
    VkDescriptorSetLayoutCreateInfo setinfo = Vk::Structures::descriptorset_layout_create_info(bindings);
    if(vkCreateDescriptorSetLayout(device, &setinfo, nullptr, &offscreenGlobalSetLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create descriptor set layout");
    }
    _mainDeletionQueue.push_function([=](){vkDestroyDescriptorSetLayout(device, offscreenGlobalSetLayout, nullptr);});


    VkDescriptorSetAllocateInfo globalSetAllocInfo = Vk::Structures::descriptorset_allocate_info(descriptorPool, &offscreenGlobalSetLayout);
    if(vkAllocateDescriptorSets(device, &globalSetAllocInfo, &offscreenDescriptorSet) != VK_SUCCESS){
        throw std::runtime_error("Failed to allocate memory for offscreen descriptor set");
    }

    //we will create the descriptor set layout for the lights, could maybe be combined with materials? or object?
    //bind light storage buffer to 0 in frag
    VkDescriptorSetLayoutBinding pointLightsBind = Vk::Structures::descriptorset_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    VkDescriptorSetLayoutBinding spotLightsBind = Vk::Structures::descriptorset_layout_binding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    bindings.clear();
    bindings = {pointLightsBind, spotLightsBind};
	setinfo = Vk::Structures::descriptorset_layout_create_info(bindings);
    if(vkCreateDescriptorSetLayout(device, &setinfo, nullptr, &os_lightSetLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create descriptor set layout");
    }
    _mainDeletionQueue.push_function([=](){vkDestroyDescriptorSetLayout(device, os_lightSetLayout, nullptr);});

    //allocate a uniform lighting descriptor
    VkDescriptorSetAllocateInfo lightingSetAllocInfo = Vk::Structures::descriptorset_allocate_info(descriptorPool, &os_lightSetLayout);
    if(vkAllocateDescriptorSets(device, &lightingSetAllocInfo, &os_lightSet) != VK_SUCCESS){
        throw std::runtime_error("Failed to allocate memory for lighting descriptor set");
    }

     //bind object storage buffer to 0 in vertex
    VkDescriptorSetLayoutBinding objectBind = Vk::Structures::descriptorset_layout_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr);
    bindings.clear();
    bindings = {objectBind};
	setinfo = Vk::Structures::descriptorset_layout_create_info(bindings);
    if(vkCreateDescriptorSetLayout(device, &setinfo, nullptr, &os_objectSetLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create descriptor set layout");
    }
    _mainDeletionQueue.push_function([=](){vkDestroyDescriptorSetLayout(device, os_objectSetLayout, nullptr);});

    //allocate a storage object descriptor set for each frame
    VkDescriptorSetAllocateInfo objectSetAllocInfo = Vk::Structures::descriptorset_allocate_info(descriptorPool, &_objectSetLayout);
    if(vkAllocateDescriptorSets(device, &objectSetAllocInfo, &os_objectDescriptor) != VK_SUCCESS){
        throw std::runtime_error("Failed to allocate memory for object descriptor set");
    }

    //shadowmap //NOT USED ATM
    /*VkDescriptorSetLayoutBinding lightVPBufferBinding = Vk::Structures::descriptorset_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr);
    VkDescriptorSetLayoutBinding shadowmapSamplerSetBinding = Vk::Structures::descriptorset_layout_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    //bindings =  {lightVPBufferBinding};
    bindings =  {lightVPBufferBinding, shadowmapSamplerSetBinding};
    setinfo = Vk::Structures::descriptorset_layout_create_info(bindings);
    if(vkCreateDescriptorSetLayout(device, &setinfo, nullptr, &lightVPSetLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create lightVPSetLayout descriptor set layout");
    }
    _mainDeletionQueue.push_function([=](){vkDestroyDescriptorSetLayout(device, lightVPSetLayout, nullptr);});
    //shadowmap
    */

    //allocate a uniform light VP matrix descriptor set for each frame
    /*VkDescriptorSetAllocateInfo lightVPSetAllocInfo = Vk::Structures::descriptorset_allocate_info(descriptorPool, &lightVPSetLayout);
    if(vkAllocateDescriptorSets(device, &lightVPSetAllocInfo, &_frames[i].lightVPSet) != VK_SUCCESS){
        throw std::runtime_error("Failed to allocate memory for light VP descriptor set");
    }*/

    std::array<VkWriteDescriptorSet, 5> setWrite{};
    //information about the buffer we want to point at in the descriptor
    VkDescriptorBufferInfo cameraInfo;
    cameraInfo.buffer = offscreenCameraBuffer; //it will be the camera buffer
    cameraInfo.offset = 0; //at 0 offset
    cameraInfo.range = sizeof(GPUCameraData); //of the size of a camera data struct
    //binding camera uniform buffer to 0
    setWrite[0] = Vk::Structures::write_descriptorset(0, offscreenDescriptorSet, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &cameraInfo);

    VkDescriptorBufferInfo sceneInfo;
    sceneInfo.buffer = offscreenSceneParameterBuffer;
    sceneInfo.offset = 0; //we are using dynamic uniform buffer now so we dont hardcode the offset, 
    //instead we tell it the offset when binding the descriptorset at drawFrame (when we need to rebind it) hence dynamic
    //and they will be reading from the same buffer
    sceneInfo.range = sizeof(GPUSceneData);
    //binding scene uniform buffer to 1, we are using dynamic offsets so set the flag
    setWrite[1] = Vk::Structures::write_descriptorset(1, offscreenDescriptorSet, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, &sceneInfo);

    VkDescriptorBufferInfo pointlightsBufferInfo;
    pointlightsBufferInfo.buffer = _frames[0].pointLightParameterBuffer; //os_pointLightParameterBuffer
    pointlightsBufferInfo.offset = 0;
    pointlightsBufferInfo.range = sizeof(GPUPointLightData) * MAX_LIGHTS;
    //notice we bind to 0 as this is part of a seperate descriptor set
    setWrite[2] = Vk::Structures::write_descriptorset(0, os_lightSet, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &pointlightsBufferInfo);

    VkDescriptorBufferInfo spotLightsBufferInfo;
    spotLightsBufferInfo.buffer = os_spotLightParameterBuffer;
    spotLightsBufferInfo.offset = 0;
    spotLightsBufferInfo.range = sizeof(GPUSpotLightData) * MAX_LIGHTS;
    setWrite[3] = Vk::Structures::write_descriptorset(1, os_lightSet, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &spotLightsBufferInfo);

    //object buffer info describes the location and the size
    VkDescriptorBufferInfo objectBufferInfo;
	objectBufferInfo.buffer = os_objectBuffer;
	objectBufferInfo.offset = 0;
	objectBufferInfo.range = sizeof(GPUObjectData) * MAX_OBJECTS;
    //notice we bind to 0 as this is part of a seperate descriptor set
    setWrite[4] = Vk::Structures::write_descriptorset(0, os_objectDescriptor, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &objectBufferInfo);

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(setWrite.size()), setWrite.data(), 0, nullptr);    
}

void Vk::OffscreenRenderer::createOffscreenImageBuffer(){
    VkBufferCreateInfo createinfo = {};
    createinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createinfo.size = OUTPUT_IMAGE_WH * OUTPUT_IMAGE_WH * 4 * sizeof(int8_t);
    createinfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    createinfo.sharingMode = VK_SHARING_MODE_CONCURRENT;

    //create the image copy buffer
    createBuffer(createinfo.size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU, offscreenImageBuffer, offscreenImageBufferAlloc);

    _swapDeletionQueue.push_function([=](){vmaDestroyBuffer(allocator, offscreenImageBuffer, offscreenImageBufferAlloc);});
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

//we will create colour resources here, this is used for MSAA and creates a single colour image that can be drawn to for calculating msaa 
void Vk::OffscreenRenderer::createOffscreenColourResources(){
    VkFormat colorFormat = swapChainImageFormat;

    imageHelper->createImage(RENDERED_IMAGE_WIDTH, RENDERED_IMAGE_HEIGHT, 1, 1, msaaSamples, (VkImageCreateFlagBits)0, colorFormat, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, offscreenColourImage,
        offscreenColourImageAllocation, VMA_MEMORY_USAGE_GPU_ONLY);

    offscreenColourImageView = imageHelper->createImageView(offscreenColourImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1);
    
    _swapDeletionQueue.push_function([=](){vmaDestroyImage(allocator, offscreenColourImage, offscreenColourImageAllocation);});
    _swapDeletionQueue.push_function([=](){vkDestroyImageView(device, offscreenColourImageView, nullptr);});
}

//create depth image
void Vk::OffscreenRenderer::createOffscreenDepthResources(){
    VkFormat depthFormat = Vk::Init::findDepthFormat(physicalDevice);
    imageHelper->createImage(RENDERED_IMAGE_WIDTH, RENDERED_IMAGE_HEIGHT, 1, 1, msaaSamples, (VkImageCreateFlagBits)0, depthFormat, VK_IMAGE_TILING_OPTIMAL, 
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, offscreenDepthImage, offscreenDepthImageAllocation, VMA_MEMORY_USAGE_GPU_ONLY);

    offscreenDepthImageView = imageHelper->createImageView(offscreenDepthImage, VK_IMAGE_VIEW_TYPE_2D, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1);
    
    _swapDeletionQueue.push_function([=](){vmaDestroyImage(allocator, offscreenDepthImage, offscreenDepthImageAllocation);});
    _swapDeletionQueue.push_function([=](){vkDestroyImageView(device, offscreenDepthImageView, nullptr);});
}

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

    std::vector<VkImageView> attachments = {offscreenColourImageView, offscreenDepthImageView, offscreenImageView};

    VkExtent2D extent{};
    extent.height = RENDERED_IMAGE_HEIGHT;
    extent.width = RENDERED_IMAGE_WIDTH;

    VkFramebufferCreateInfo framebufferInfo = Vk::Structures::framebuffer_create_info(offscreenRenderPass, attachments, extent, 1);

    if(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &offscreenFramebuffer) != VK_SUCCESS){
        throw std::runtime_error("Failed to create offscreen framebuffer");
    }
    _swapDeletionQueue.push_function([=](){vkDestroyFramebuffer(device, offscreenFramebuffer, nullptr);});
}

void Vk::OffscreenRenderer::createOffscreenImageAndView(){
    //VkFormat imageFormat = VK_FORMAT_R8_UNORM;
    VkFormat imageFormat = swapChainImageFormat;

    //used as target for main offscreen rendering pass
    imageHelper->createImage(RENDERED_IMAGE_WIDTH, RENDERED_IMAGE_HEIGHT, 1, 1, VK_SAMPLE_COUNT_1_BIT, (VkImageCreateFlagBits)0, imageFormat, VK_IMAGE_TILING_OPTIMAL, 
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, offscreenImage, offscreenImageAllocation, VMA_MEMORY_USAGE_GPU_ONLY);
    _swapDeletionQueue.push_function([=](){vmaDestroyImage(allocator, offscreenImage, offscreenImageAllocation);});
    offscreenImageView = imageHelper->createImageView(offscreenImage, VK_IMAGE_VIEW_TYPE_2D, imageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1);
    _swapDeletionQueue.push_function([=](){vkDestroyImageView(device, offscreenImageView, nullptr);});

    //copy image used as a crop to reduce RENDERED_IMAGE_WIDTH to OUTPUT_IMAGE_WH
    imageHelper->createImage(OUTPUT_IMAGE_WH, OUTPUT_IMAGE_WH, 1, 1, VK_SAMPLE_COUNT_1_BIT, (VkImageCreateFlagBits)0, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, 
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, cropImage, cropImageAllocation, 
                    VMA_MEMORY_USAGE_GPU_ONLY);
    _swapDeletionQueue.push_function([=](){vmaDestroyImage(allocator, cropImage, cropImageAllocation);});

    //blit image from cropImage VK_FORMAT_B8G8R8A8_SRGB to this VK_FORMAT_B8G8R8A8_UNORM format
    //this will be passed to Imgui for drawing as a texture
    imageHelper->createImage(OUTPUT_IMAGE_WH, OUTPUT_IMAGE_WH, 1, 1, VK_SAMPLE_COUNT_1_BIT, (VkImageCreateFlagBits)0, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, 
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, greyRGBImage, greyRGBImageAllocation, 
                    VMA_MEMORY_USAGE_GPU_ONLY);
    _swapDeletionQueue.push_function([=](){vmaDestroyImage(allocator, greyRGBImage, greyRGBImageAllocation);});

    
    for(int i = 0; i < opticsTextures.size(); i++){
        //create textures and views for optics, for use in imgui
        imageHelper->createImage(OUTPUT_IMAGE_WH, OUTPUT_IMAGE_WH, 1, 1, VK_SAMPLE_COUNT_1_BIT, (VkImageCreateFlagBits)0, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, 
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, opticsTextures[i].image, opticsTextures[i].alloc, 
                    VMA_MEMORY_USAGE_GPU_ONLY);
        _swapDeletionQueue.push_function([=](){vmaDestroyImage(allocator, opticsTextures[i].image, opticsTextures[i].alloc);});

        opticsTextures[i].imageView = imageHelper->createImageView(opticsTextures[i].image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1);
        _swapDeletionQueue.push_function([=](){vkDestroyImageView(device, opticsTextures[i].imageView, nullptr);});


        //create textures and views for detection, for use in imgui
        imageHelper->createImage(OUTPUT_IMAGE_WH, OUTPUT_IMAGE_WH, 1, 1, VK_SAMPLE_COUNT_1_BIT, (VkImageCreateFlagBits)0, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_TILING_LINEAR, 
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, detectionTextures[i].image, detectionTextures[i].alloc, 
                    VMA_MEMORY_USAGE_CPU_ONLY);
        _swapDeletionQueue.push_function([=](){vmaDestroyImage(allocator, detectionTextures[i].image, detectionTextures[i].alloc);});

        detectionTextures[i].imageView = imageHelper->createImageView(detectionTextures[i].image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1);
        _swapDeletionQueue.push_function([=](){vkDestroyImageView(device, detectionTextures[i].imageView, nullptr);});

        //mapping the detection images memory
        VkImageSubresource subResource { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
        VkSubresourceLayout subResourceLayout;
        vkGetImageSubresourceLayout(device, detectionTextures[i].image, &subResource, &subResourceLayout);
        vmaMapMemory(allocator, detectionTextures[i].alloc, (void**)&detectionImageMappings[i]);
        detectionImageMappings[i] += subResourceLayout.offset;
    
    }

    for(int i = 0; i < opticsTextures.size(); i++){
        imguiTexturePackets[i].p_layout = VK_IMAGE_LAYOUT_GENERAL; //will be general after transition
        imguiTexturePackets[i].p_view = &opticsTextures[i].imageView; //will be general after transition
        imguiTexturePackets[i].p_sampler = &greyRGBImageSampler;
    }

    for(int i = 0; i < opticsTextures.size(); i++){
        int ind = i + opticsTextures.size();
        //offset into imguiTexturePackets by size of previous loop
        imguiTexturePackets[ind].p_layout = VK_IMAGE_LAYOUT_GENERAL; //will be general after transition
        imguiTexturePackets[ind].p_view = &detectionTextures[i].imageView; //will be general after transition
        imguiTexturePackets[ind].p_sampler = &greyRGBImageSampler;
    }

    imageHelper->createImage(OUTPUT_IMAGE_WH, OUTPUT_IMAGE_WH, 1, 1, VK_SAMPLE_COUNT_1_BIT, (VkImageCreateFlagBits)0, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_TILING_LINEAR, 
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, dstImage, dstImageAllocation, 
                    VMA_MEMORY_USAGE_CPU_ONLY);
    _swapDeletionQueue.push_function([=](){vmaDestroyImage(allocator, dstImage, dstImageAllocation);});

    //mapping this one image for now, perma mapped, unmapped in cleanup
    VkImageSubresource subResource { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
    VkSubresourceLayout subResourceLayout;
    vkGetImageSubresourceLayout(device, dstImage, &subResource, &subResourceLayout);
    vmaMapMemory(allocator, dstImageAllocation, (void**)&dstImageMappedData);
    dstImageMappedData += subResourceLayout.offset;

    //create textures and views for detection, for use in imgui
        imageHelper->createImage(OUTPUT_IMAGE_WH*2, OUTPUT_IMAGE_WH, 1, 1, VK_SAMPLE_COUNT_1_BIT, (VkImageCreateFlagBits)0, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_TILING_LINEAR, 
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, matchTexture.image, matchTexture.alloc, 
                    VMA_MEMORY_USAGE_CPU_ONLY);
        _swapDeletionQueue.push_function([=](){vmaDestroyImage(allocator, matchTexture.image, matchTexture.alloc);});

        matchTexture.imageView = imageHelper->createImageView(matchTexture.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1);
        _swapDeletionQueue.push_function([=](){vkDestroyImageView(device, matchTexture.imageView, nullptr);});

    //mapping this one image for now, perma mapped, unmapped in cleanup
    //subResource { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
    //VkSubresourceLayout subResourceLayout;
    vkGetImageSubresourceLayout(device, matchTexture.image, &subResource, &subResourceLayout);
    vmaMapMemory(allocator, matchTexture.alloc, (void**)&matchImageMapping);
    matchImageMapping += subResourceLayout.offset;

    imguiTexturePackets[4].p_layout = VK_IMAGE_LAYOUT_GENERAL; //will be general after transition
    imguiTexturePackets[4].p_view = &matchTexture.imageView; //will be general after transition
    imguiTexturePackets[4].p_sampler = &greyRGBImageSampler;


    
}

//notes here are mostly from https://raw.githubusercontent.com/Overv/VulkanTutorial/master/ebook/Vulkan%20Tutorial%20en.pdf
//these are left in for reference
void Vk::OffscreenRenderer::createOffscreenRenderPass(){
    //in our case we will need a colour buffer represented by one of the images from the swap chain
    //VkFormat imageFormat = VK_FORMAT_R8_UNORM;
    VkFormat imageFormat = swapChainImageFormat;

    VkAttachmentDescription colourAttachment = Vk::Structures::attachment_description(imageFormat, msaaSamples, VK_ATTACHMENT_LOAD_OP_CLEAR, 
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

    VkAttachmentDescription colourAttachmentResolve = Vk::Structures::attachment_description(imageFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_DONT_CARE, 
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
}

//take the last VKImage output by offscreen pass, convert it to linear format so we can read it
//adapted from Sascha Willems screenshot example https://github.com/SaschaWillems/Vulkan/blob/master/examples/screenshot/screenshot.cpp
void Vk::OffscreenRenderer::convertOffscreenImage(){
    opticsFrameCounter = (opticsFrameCounter + 1) % NUM_TEXTURE_SETS;
        
    VkCommandBufferAllocateInfo cmdBufAllocateInfo{};
    cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufAllocateInfo.commandPool = offscreenCommandPool;
    cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufAllocateInfo.commandBufferCount = 1;

    VkCommandBuffer cmdBuffer;
    vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &cmdBuffer);
    VkCommandBufferBeginInfo cmdBufInfo = Vk::Structures::command_buffer_begin_info(0);
    vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo);

    std::scoped_lock<std::mutex> lock(copyLock);

    //prepare grey transition image
    imageHelper->insertImageMemoryBarrier(
        cmdBuffer,
        cropImage,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    VkImageCopy imageCopyRegion{};
    imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopyRegion.srcSubresource.layerCount = 1;
    imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopyRegion.dstSubresource.layerCount = 1;
    imageCopyRegion.extent.width = OUTPUT_IMAGE_WH;
    imageCopyRegion.extent.height = OUTPUT_IMAGE_WH;
    imageCopyRegion.extent.depth = 1;
    imageCopyRegion.srcOffset.x = OFFSCREEN_IMAGE_WIDTH_OFFSET;
    imageCopyRegion.srcOffset.y = OFFSCREEN_IMAGE_HEIGHT_OFFSET;

    // Issue the copy command
    vkCmdCopyImage(
        cmdBuffer,
        offscreenImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        cropImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &imageCopyRegion);

    imageHelper->insertImageMemoryBarrier(
        cmdBuffer,
        cropImage,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    imageHelper->insertImageMemoryBarrier(
        cmdBuffer,
        greyRGBImage,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    // Define the region to blit (we will blit the whole swapchain image)
    VkOffset3D blitSize;
    blitSize.x = OUTPUT_IMAGE_WH;
    blitSize.y = OUTPUT_IMAGE_WH;
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
        cropImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        greyRGBImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &imageBlitRegion, 
        VK_FILTER_NEAREST);

    imageHelper->insertImageMemoryBarrier(
        cmdBuffer,
        greyRGBImage,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    //remove last from the queue, this means we wont render it during copy
    if(imguiTextureSetIndicesQueue.size() >= NUM_TEXTURE_SETS)
        imguiTextureSetIndicesQueue.pop_front();

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

    imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopyRegion.srcSubresource.layerCount = 1;
    imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopyRegion.dstSubresource.layerCount = 1;
    imageCopyRegion.extent.width = OUTPUT_IMAGE_WH;
    imageCopyRegion.extent.height = OUTPUT_IMAGE_WH;
    imageCopyRegion.extent.depth = 1;
    imageCopyRegion.srcOffset.x = 0;
    imageCopyRegion.srcOffset.y = 0;

    // Issue the copy command
    vkCmdCopyImage(
        cmdBuffer,
        greyRGBImage, //opticsTextures[opticsFrameCounter].image, //greyRGBImage, 
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &imageCopyRegion);

    imageHelper->insertImageMemoryBarrier(
        cmdBuffer,
        opticsTextures[opticsFrameCounter].image,//opticsTextures[opticsFrameCounter].image, //greyRGBImage,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    // Issue the copy command
    vkCmdCopyImage(
        cmdBuffer,
        greyRGBImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        opticsTextures[opticsFrameCounter].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &imageCopyRegion);

    imageHelper->insertImageMemoryBarrier(
        cmdBuffer,
        opticsTextures[opticsFrameCounter].image, //greyRGBImage,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    //gui image is now ready to show again
    imguiTextureSetIndicesQueue.push_back(opticsFrameCounter);
    //this is only for the optics texture though, might have to make 2 of these indices queues?

    imageHelper->insertImageMemoryBarrier(
        cmdBuffer,
        dstImage,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    imageHelper->insertImageMemoryBarrier(
        cmdBuffer,
        detectionTextures[opticsFrameCounter].image, //greyRGBImage,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    //match texture and optics textures, and maybe dst can be transitioned once before this, saving time
    imageHelper->insertImageMemoryBarrier(
        cmdBuffer,
        matchTexture.image, //greyRGBImage,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    flushCommandBuffer(cmdBuffer, graphicsQueue, offscreenCommandPool, false);    

    //wrap the mapped memory to an opencv mat
    cv::Mat wrappedMat = cv::Mat(OUTPUT_IMAGE_WH, OUTPUT_IMAGE_WH, CV_8UC4, (void*)dstImageMappedData, cv::Mat::AUTO_STEP);
    cvMatQueue.push_back(wrappedMat);
}

//used to assign material of feature detection image, used to render opencv output on ui
void Vk::OffscreenRenderer::assignMatToDetectionView(cv::Mat image){
    if(imguiDetectionIndicesQueue.size() >= NUM_TEXTURE_SETS)
        imguiDetectionIndicesQueue.pop_front();

    size_t sizeInBytes = image.step[0] * image.rows;
    memcpy((void*)detectionImageMappings[opticsFrameCounter], image.data, sizeInBytes);

    imguiDetectionIndicesQueue.push_back(opticsFrameCounter);
}

//used to assign material of feature matching image, used to render opencv output on ui
void Vk::OffscreenRenderer::assignMatToMatchingView(cv::Mat image){
    if(imguiMatchIndicesQueue.size() >= 1)
        imguiMatchIndicesQueue.pop_front();

    size_t sizeInBytes = image.step[0] * image.rows;
    memcpy((void*)matchImageMapping, image.data, sizeInBytes);

    imguiMatchIndicesQueue.push_back(0); //index will always be 0, although value doesnt actually matter for this one as its just used as a bool in ui
}

void Vk::OffscreenRenderer::clearOpticsViews(){
    //careful of mem leaks, need to check
    imguiMatchIndicesQueue.clear();
    imguiDetectionIndicesQueue.clear();
    imguiTextureSetIndicesQueue.clear();
}

void Vk::OffscreenRenderer::setShouldDrawOffscreen(bool b){
    shouldDrawOffscreenFrame = b;
}

//update the lander camera pos and orientation data for passing to shaders
void Vk::OffscreenRenderer::populateLanderCameraData(GPUCameraData& camData){
    RenderObject* lander =  p_renderables->at(2).get(); //lander is obj 2
    glm::vec3 camPos = lander->pos;

    glm::mat4 view = glm::lookAt(camPos, camPos - lander->up, lander->forward); //setting view to look forward

    glm::mat4 proj = glm::perspective(glm::radians(OFFSCREEN_IMAGE_FOV), (float)RENDERED_IMAGE_WIDTH / (float)RENDERED_IMAGE_HEIGHT, 0.1f, 15000.0f);
    proj[1][1] *= -1; //glm and vulkan Y axis are inverted

    camData.projection = proj;
    camData.view = view;
	camData.viewproj = proj * view;
}

void Vk::OffscreenRenderer::drawFrame(){
    if (shouldDrawOffscreenFrame){
        std::scoped_lock<std::mutex> lock(copyLock); //must wait for any in progress copy operation
        shouldDrawOffscreenFrame = false;
        recordCommandBuffer_Offscreen();
        renderSubmitted = true;
    }

    if(renderSubmitted){//this just stops us checking for fence if we know a new render wasnt submitted since the last one
        if(vkGetFenceStatus(device, offscreenCopyFence) == VK_SUCCESS){ //if offscreen render fence has been signalled then
            std::thread thread(&Vk::OffscreenRenderer::convertOffscreenImage, this); //we run the conversions in a seperate thread, reduces stuttering
            thread.detach();   
            //convertOffscreenImage();
            renderSubmitted = false;
        }
    }

    Renderer::drawFrame(); //draw a frame
    //Renderer::drawDebugFrame(); //draw debug frame, not implemented yet
}

void Vk::OffscreenRenderer::recordCommandBuffer_Offscreen(){
    //wait for any rendering to complete here
    vkWaitForFences(device, 1, &offscreenCopyFence, VK_TRUE, UINT64_MAX); //wait for fence to be signalled
    vkResetFences(device, 1, &offscreenCopyFence); //set to unsignalled

    vkResetCommandBuffer(offscreenCommandBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT); 

    VkCommandBufferBeginInfo beginInfo = Vk::Structures::command_buffer_begin_info(0);
    if(vkBeginCommandBuffer(offscreenCommandBuffer, &beginInfo) != VK_SUCCESS){
        throw std::runtime_error("Failed to begin recording offscreen command buffer");
    }

    VkExtent2D extent{RENDERED_IMAGE_WIDTH, RENDERED_IMAGE_HEIGHT}; 

    //we need to use our own renderpass here, with offscreen 
    VkRenderPassBeginInfo renderPassBeginInfo = Vk::Structures::render_pass_begin_info(offscreenRenderPass, offscreenFramebuffer, {0, 0}, extent, clearValues);

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
    std::scoped_lock<std::mutex> lock(queueSubmitMutex); //wait for any other queue submissions, should use scoped_lock elsewhere too. c++17 recommended practice
    if(vkQueueSubmit(graphicsQueue, 1 , &submitInfo, offscreenCopyFence) != VK_SUCCESS) { //signal fence on completion
        throw std::runtime_error("Failed to submit draw command buffer");
    }
}

//temp overridden for testing optical settings, actually easier to change scene lighting than camera sensitivity
void Vk::OffscreenRenderer::updateSceneData(GPUCameraData& camData){
    glm::vec3 lightDir = glm::vec3(camData.view * glm::vec4(p_sceneLight->pos, 0));
    _sceneParameters.lightDirection = glm::vec4(lightDir, 0);
    _sceneParameters.lightAmbient = LANDER_OPTICS_AMBIENT;
    _sceneParameters.lightDiffuse = LANDER_OPTICS_DIFFUSE;
    _sceneParameters.lightSpecular = LANDER_OPTICS_SPECULAR;
    //_sceneParameters.lightSpecular = glm::vec4(p_sceneLight->specular, 1);

    // Compute the MVP matrix from the light's point of view, ortho projection, need to adjust values
    glm::mat4 projectionMatrix = glm::ortho<float>(-1000,1000,-1000,1000,-1000,1000);
    glm::mat4 viewMatrix = glm::lookAt(lightDir, glm::vec3(0,0,0), glm::vec3(0,1,0));
    glm::mat4 modelMatrix = glm::mat4(1.0);
    lightVPParameters[p_sceneLight->layer].viewproj = projectionMatrix * viewMatrix * modelMatrix;
    //this is used to render the scene from the lights POV to generate a shadow map, not implemented yet
}

//point lights and spotlights
void Vk::OffscreenRenderer::updateLightingData(GPUCameraData& camData){
    for(int i = 0; i < p_pointLights->size(); i++){
        _pointLightParameters[i].position = glm::vec3(camData.view * glm::vec4(p_pointLights->at(i).pos, 1));
        _pointLightParameters[i].diffuse = p_pointLights->at(i).diffuse;
        _pointLightParameters[i].ambient = p_pointLights->at(i).ambient;
        _pointLightParameters[i].specular = p_pointLights->at(i).specular;
        _pointLightParameters[i].attenuation = p_pointLights->at(i).attenuation;
        //not going to implement pointlight shadow maps.
        //When we do want to we will need to create a shadow cube map per light instead of a 2d texture (because point lights emit in all directions)
    }
   
    for(int i = 0; i < p_spotLights->size(); i++){
        _spotLightParameters[i].position = glm::vec3(camData.view * glm::vec4(p_spotLights->at(i).pos, 1));
        _spotLightParameters[i].diffuse = p_spotLights->at(i).diffuse;
        _spotLightParameters[i].ambient = p_spotLights->at(i).ambient;
        _spotLightParameters[i].specular = p_spotLights->at(i).specular;
        _spotLightParameters[i].attenuation = p_spotLights->at(i).attenuation;
        _spotLightParameters[i].direction = glm::vec3(camData.view * glm::vec4(p_spotLights->at(i).direction, 0));
        _spotLightParameters[i].cutoffs = p_spotLights->at(i).cutoffs;

        //mvpMatrix for shadowmap, perspective projection
        /*glm::mat4 clip = glm::mat4(1.0f, 0.0f, 0.0f, 0.0f,
                           0.0f,-1.0f, 0.0f, 0.0f,
                           0.0f, 0.0f, 0.5f, 0.0f,
                           0.0f, 0.0f, 0.5f, 1.0f);

        glm::mat4 projectionMatrix = clip * glm::perspective<float>(glm::radians(p_spotLights->at(i).cutoffAngles.y), 1.0f,  p_spotLights->at(i).cutoffAngles.x, p_spotLights->at(i).cutoffAngles.y);
        
        glm::mat4 viewMatrix = glm::lookAt(p_spotLights->at(i).pos, p_spotLights->at(i).pos - p_spotLights->at(i).direction, glm::vec3(0,1,0));
        glm::mat4 modelMatrix = glm::mat4(1.0);
        
        lightVPParameters[p_sceneLight->layer].viewproj = projectionMatrix * viewMatrix * modelMatrix;*/
    }
}

std::vector<ImguiTexturePacket>& Vk::OffscreenRenderer::getDstTexturePackets(){
    return imguiTexturePackets;
}

void Vk::OffscreenRenderer::popCvMatQueue(){
    std::scoped_lock<std::mutex> lock(queueSubmitMutex); 
    cvMatQueue.pop_front(); 
}

cv::Mat& Vk::OffscreenRenderer::frontCvMatQueue(){
    std::scoped_lock<std::mutex> lock(queueSubmitMutex); 
    return cvMatQueue.front();
}

void Vk::OffscreenRenderer::mapLightingDataToGPU(){
    //for(int i = 0; i < swapChainImages.size(); i++){
        //copy current point light data array into buffer
        //char* pointLightingData;
        //vmaMapMemory(allocator, _frames[0].pointLightParameterBufferAlloc , (void**)&pointLightingData);
        //memcpy(pointLightingData, &_pointLightParameters, sizeof(_pointLightParameters));
        //vmaUnmapMemory(allocator, _frames[0].pointLightParameterBufferAlloc); 
    //}
    //copy current spot light data array into buffer
    char* spotLightingData;
    vmaMapMemory(allocator, os_spotLightParameterBufferAlloc , (void**)&spotLightingData);
    memcpy(spotLightingData, _spotLightParameters, sizeof(_spotLightParameters));
    vmaUnmapMemory(allocator, os_spotLightParameterBufferAlloc); 

    //now for the lightVP matrix uniform buffer
    /*for(int i = 0; i < swapChainImages.size(); i++){
        //copy current point light data array into buffer
        char* lightVPData;
        vmaMapMemory(allocator, _frames[i].lightVPBufferAlloc , (void**)&lightVPData);
        memcpy(lightVPData, &lightVPParameters, sizeof(lightVPParameters));
        vmaUnmapMemory(allocator, _frames[i].lightVPBufferAlloc); 
    }*/
}

//actual scene drawing
void Vk::OffscreenRenderer::drawOffscreen(int curFrame){
    ////fill a GPU camera data struct
	GPUCameraData camData;
    populateLanderCameraData(camData);
    updateSceneData(camData); //these

    //need our own lighting data buffer as well
    updateLightingData(camData);
    mapLightingDataToGPU(); //map to gpu
    
    //and copy it to the buffer
	void* data;
	vmaMapMemory(allocator, offscreenCameraBufferAllocation, &data);
	memcpy(data, &camData, sizeof(GPUCameraData));
	vmaUnmapMemory(allocator, offscreenCameraBufferAllocation);

    char* sceneData;
    vmaMapMemory(allocator, offscreenSceneParameterBufferAlloc , (void**)&sceneData);
	memcpy(sceneData, &_sceneParameters, sizeof(GPUSceneData));
	vmaUnmapMemory(allocator, offscreenSceneParameterBufferAlloc); 

    //then do the objects data into the storage buffer
    void* objectData;
    vmaMapMemory(allocator, os_objectBufferAlloc, &objectData);
    GPUObjectData* objectSSBO = (GPUObjectData*)objectData;
    //loop through all objects in the scene, need a better container than a vector
    for (int i = 0; i < p_renderables->size(); i++){
        objectSSBO[i].modelMatrix = p_renderables->at(i)->transformMatrix;
        //calculate the normal matrix, its done by inverse transpose of the model matrix * view matrix because we are working 
        //in view space in the shader. taking only the top 3x3
        //this is to account for rotation and scaling when using normals. should be done on cpu as its costly
        objectSSBO[i].normalMatrix = glm::transpose(glm::inverse(camData.view * p_renderables->at(i)->transformMatrix)); 
    }
    vmaUnmapMemory(allocator, os_objectBufferAlloc);

    Material* lastMaterial = nullptr;

    //could have a menu option to disable drawing certain things in offscreen like the asteroid for testing,
    //or in the main renderer pass show things like landing site boxes and other debugging things
    std::vector<int> renderObjectIds;
    //renderObjectIds = std::vector{1,3,4,5,6,7,11}; //1 = star, 3 = asteroid, 4-7 = landing site boxes
    renderObjectIds = std::vector{1,3}; //1 = star, 3 = asteroid, 4-7 = landing site boxes
    
    //here we are drawing objects using altMaterial, this is the greyscale material of each respective object set in scene init
    for(const int i : renderObjectIds){
        RenderObject* object = p_renderables->at(i).get();
        //only bind the pipeline and different descriptor sets if they don't match the already bound one
		if (object->altMaterial != lastMaterial) {
            if(object->altMaterial == nullptr){throw std::runtime_error("object.material is a null reference in drawObjects()");} //remove if statement in release
			vkCmdBindPipeline(offscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object->altMaterial->pipeline);
			lastMaterial = object->altMaterial;

            //offset for our scene buffer
            uint32_t scene_uniform_offset = 0; //requires offset for dynamic offscreenDescriptorSet, because its built on renderer pipeline layout, must match
            //bind the descriptor set when changing pipeline
            vkCmdBindDescriptorSets(offscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                object->altMaterial->pipelineLayout, 0, 1, &offscreenDescriptorSet, 1, &scene_uniform_offset); //pass offscreen descriptor set here which holds lander cam data as well as normal scene data

	        //object data descriptor
        	vkCmdBindDescriptorSets(offscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                object->altMaterial->pipelineLayout, 1, 1, &os_objectDescriptor, 0, nullptr);

             //material descriptor, we are using a dynamic uniform buffer and referencing materials by their offset with propertiesId, which just stores the int relative to the material in _materials
        	vkCmdBindDescriptorSets(offscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                object->altMaterial->pipelineLayout, 2, 1, &_frames[curFrame].materialSet, 0, nullptr);
            //the "firstSet" param above is 2 because in init_pipelines its described in VkDescriptorSetLayout[] in index 2!
            
            vkCmdBindDescriptorSets(offscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                object->altMaterial->pipelineLayout, 3, 1, &os_lightSet, 0, nullptr);

            if (object->altMaterial->_multiTextureSets.size() > 0) {
                vkCmdBindDescriptorSets(offscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                    object->altMaterial->pipelineLayout, 4, 1, &object->altMaterial->_multiTextureSets[0], 0, nullptr);
		    }
		}
        
        //add material property id for this object to push constant
		PushConstants constants;
		constants.matIndex = object->altMaterial->propertiesId;
        constants.numPointLights = p_pointLights->size();
        constants.numSpotLights = p_spotLights->size();

        //upload the mat id to the GPU via pushconstants
		vkCmdPushConstants(offscreenCommandBuffer, object->altMaterial->pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &constants);

        vkCmdDrawIndexed(offscreenCommandBuffer, _loadedMeshes[object->meshId].indexCount, 1, _loadedMeshes[object->meshId].indexBase, 0, i); //using i as index for storage buffer in shaders
    }

    //here we are drawing objects using material, this is the coloured material
    /*for(const int i : renderObjectIds){
        RenderObject* object = p_renderables->at(i).get();
        //only bind the pipeline and different descriptor sets if they don't match the already bound one
		if (object->material != lastMaterial) {
            if(object->material == nullptr){throw std::runtime_error("object.material is a null reference in drawObjects()");} //remove if statement in release
			vkCmdBindPipeline(offscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object->material->pipeline);
			lastMaterial = object->material;

            //offset for our scene buffer
            uint32_t scene_uniform_offset = 0; //requires offset for dynamic offscreenDescriptorSet, because its built on renderer pipeline layout, must match
            //bind the descriptor set when changing pipeline
            vkCmdBindDescriptorSets(offscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                object->material->pipelineLayout, 0, 1, &offscreenDescriptorSet, 1, &scene_uniform_offset); //pass offscreen descriptor set here which holds lander cam data as well as normal scene data

            vkCmdBindDescriptorSets(offscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                object->material->pipelineLayout, 0, 1, &offscreenDescriptorSet, 1, &scene_uniform_offset); //pass offscreen descriptor set here which holds lander cam data as well as normal scene data

	        //object data descriptor
        	vkCmdBindDescriptorSets(offscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                object->material->pipelineLayout, 1, 1, &os_objectDescriptor, 0, nullptr);

             //material descriptor, we are using a dynamic uniform buffer and referencing materials by their offset with propertiesId, which just stores the int relative to the material in _materials
        	vkCmdBindDescriptorSets(offscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                object->material->pipelineLayout, 2, 1, &_frames[curFrame].materialSet, 0, nullptr);
            //the "firstSet" param above is 2 because in init_pipelines its described in VkDescriptorSetLayout[] in index 2!
            
            vkCmdBindDescriptorSets(offscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                object->material->pipelineLayout, 3, 1, &os_lightSet, 0, nullptr);

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
    }*/
}

void Vk::OffscreenRenderer::cleanup(){
    vmaUnmapMemory(allocator, dstImageAllocation);
    Vk::RendererBase::cleanup();
}