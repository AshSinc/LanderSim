//#define VMA_DEBUG_LOG(format, ...) do { \
//       printf(format); \
//       printf("\n"); \
//  } while(false)
#include "vk_renderer.h"
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <optional>
#include <set>
#include <cstdint>
#include <algorithm>
#include <array>
#define GLM_FORCE_RADIANS //makes sure GLM uses radians to avoid confusion
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES //forces GLM to use a version of vec2 and mat4 that have the correct alignment requirements for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE //forces GLM to use depth range of 0 to 1, instead of -1 to 1 as in OpenGL
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL //using glm to hash Vertex attributes using a template specialisation of std::hash<T>
#include <glm/gtx/hash.hpp> //used in unnordered_map to compare vertices
#include <glm/gtc/matrix_transform.hpp> //view transformations like glm::lookAt, and projection transformations like glm::perspective glm::rotate
#define TINYOBJLOADER_IMPLEMENTATION //same as with STD, define in one source file to avoid linker errors
#include <tiny_obj_loader.h> //we will use tiny obj loader to load our meshes from file (could use fast_obj_loader.h if this is too slow)
#include "vk_structures.h"
#include "vk_images.h"
#include "world_camera.h"
#include "obj_render.h"
#include <exception>
#include "vk_pipeline.h"
#include "vk_init_queries.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <thread>

Vk::Renderer::Renderer(GLFWwindow* windowptr, Mediator& mediator): RendererBase(windowptr, mediator){}

void Vk::Renderer::init(){
    Vk::RendererBase::init();
    createOffscreenImageAndView();
    createOffscreenFramebuffer();
    createOffscreenCommandBuffer();
    createOffscreenImageBuffer();
}

void Vk::Renderer::createOffscreenImageBuffer(){
    VkBufferCreateInfo createinfo = {};
    createinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createinfo.size = OFFSCREEN_IMAGE_WIDTH * OFFSCREEN_IMAGE_HEIGHT * 4 * sizeof(int8_t);
    createinfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    createinfo.sharingMode = VK_SHARING_MODE_CONCURRENT;

    //create the image copy buffer
    createBuffer(createinfo.size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU, offscreenImageBuffer, offscreenImageBufferAlloc);

    _swapDeletionQueue.push_function([=](){vmaDestroyBuffer(allocator, offscreenImageBuffer, offscreenImageBufferAlloc);});
}

void Vk::Renderer::createOffscreenCommandBuffer(){
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

void Vk::Renderer::createOffscreenFramebuffer(){

    std::vector<VkImageView> attachments = {colourImageView, depthImageView, offscreenImageView};

    VkFramebufferCreateInfo framebufferInfo = Vk::Structures::framebuffer_create_info(renderPass, attachments, swapChainExtent, 1);

    if(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &offscreenFramebuffer) != VK_SUCCESS){
        throw std::runtime_error("Failed to create framebuffer");
    }
    _swapDeletionQueue.push_function([=](){vkDestroyFramebuffer(device, offscreenFramebuffer, nullptr);});
}

void Vk::Renderer::createOffscreenImageAndView(){
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
void Vk::Renderer::writeOffscreenImageToDisk(){
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

/*void Vk::Renderer::createShadowMapPipeline(){
    auto shadowmap_vert_c = Vk::Init::readFile("resources/shaders/shadowmap_vert.spv"); 
    auto shadowmap_vert_m = createShaderModule(shadowmap_vert_c);
    auto shadowmap_frag_test_c = Vk::Init::readFile("resources/shaders/shadowmap_frag_test.spv"); 
    auto shadowmap_frag_test_m = createShaderModule(shadowmap_frag_test_c);

    VkDescriptorSetLayout shadowmapSetLayouts[] = {lightVPSetLayout, _objectSetLayout};
    VkPipelineLayoutCreateInfo shadowmap_pipeline_layout_info = Vk::Structures::pipeline_layout_create_info(2, shadowmapSetLayouts);

    //create the layout
    shadowmapPipelineLayout;
    if(vkCreatePipelineLayout(device, &shadowmap_pipeline_layout_info, nullptr, &shadowmapPipelineLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create shadowmap pipeline layout");
    }
    _swapDeletionQueue.push_function([=](){vkDestroyPipelineLayout(device, shadowmapPipelineLayout, nullptr);});

    PipelineBuilder pipelineBuilder;

    //general setup of the pipeline
    pipelineBuilder.viewport = Vk::Structures::viewport(0.0f, 0.0f, (float) SHADOW_MAP_WIDTH, (float) SHADOW_MAP_HEIGHT, 0.0f, 1.0f);
    VkExtent2D extent{SHADOW_MAP_WIDTH,SHADOW_MAP_HEIGHT};
    pipelineBuilder.scissor = Vk::Structures::scissor(0,0,extent);
    VertexInputDescription vertexInputDescription = Vertex::get_vertex_description();
    pipelineBuilder.vertexInputInfo = Vk::Structures::pipeline_vertex_input_create_info(vertexInputDescription);
    pipelineBuilder.inputAssembly = Vk::Structures::pipeline_input_assembly_state_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
    pipelineBuilder.rasterizer = Vk::Structures::pipeline_rasterization_state_create_info(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    pipelineBuilder.multisampling = Vk::Structures::pipeline_msaa_state_create_info(VK_SAMPLE_COUNT_1_BIT); //no msaa
    pipelineBuilder.depthStencil = Vk::Structures::pipeline_depth_stencil_state_create_info(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);//VK_COMPARE_OP_LESS
    pipelineBuilder.colourBlendAttachment = Vk::Structures::pipeline_colour_blend_attachment(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, VK_FALSE);
    pipelineBuilder.pipelineLayout = shadowmapPipelineLayout;

    //add required shaders for this stage
    pipelineBuilder.shaderStages.clear();
    pipelineBuilder.shaderStages.push_back(Vk::Structures::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, shadowmap_vert_m, nullptr));
    pipelineBuilder.shaderStages.push_back(Vk::Structures::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, shadowmap_frag_test_m, nullptr));

    shadowmapPipeline = pipelineBuilder.build_pipeline(device, shadowmapRenderPass, 2);
    _swapDeletionQueue.push_function([=](){vkDestroyPipeline(device, shadowmapPipeline, nullptr);});

    vkDestroyShaderModule(device, shadowmap_frag_test_m, nullptr);
    vkDestroyShaderModule(device, shadowmap_vert_m, nullptr);
}

void Vk::Renderer::createShadowMapImage(){
    //create the layered Image with MAX_LIGHTS number of layers
    imageHelper->createImage(SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT, 1, MAX_LIGHTS, VK_SAMPLE_COUNT_1_BIT, (VkImageCreateFlagBits)0, VK_FORMAT_D32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, 
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, shadowmapImage, shadowMapImageAllocation);
    _textureDeletionQueue.push_function([=](){vmaDestroyImage(allocator, shadowmapImage, shadowMapImageAllocation);});
}

void Vk::Renderer::createShadowMapFrameBuffer(){
    VkFramebufferCreateInfo framebufferInfo;
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.pNext = NULL;
    framebufferInfo.renderPass = shadowmapRenderPass;
    framebufferInfo.attachmentCount = shadowmapImageViews.size();
    framebufferInfo.pAttachments = shadowmapImageViews.data(); //think this should be an array of VkImages
    framebufferInfo.width = SHADOW_MAP_WIDTH;
    framebufferInfo.height = SHADOW_MAP_HEIGHT;
    framebufferInfo.layers = 1;
    framebufferInfo.flags = 0;

    if(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &shadowmapFramebuffer) != VK_SUCCESS){
        throw std::runtime_error("Failed to create shadowmap framebuffer");
    }
    _swapDeletionQueue.push_function([=](){vkDestroyFramebuffer(device, shadowmapFramebuffer, nullptr);});
}

void Vk::Renderer::createShadowMapRenderPass(){
    VkAttachmentDescription attachments[2];
 
   // Depth attachment (shadow map)
   attachments[0].format = VK_FORMAT_D32_SFLOAT;
   attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
   attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
   attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
   attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
   attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
   attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   attachments[0].flags = 0;
 
   // Attachment references from subpasses
   VkAttachmentReference depth_ref;
   depth_ref.attachment = 0;
   depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
 
   // Subpass 0: shadow map rendering
   VkSubpassDescription subpass[1];
   subpass[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
   subpass[0].flags = 0;
   subpass[0].inputAttachmentCount = 0;
   subpass[0].pInputAttachments = NULL;
   subpass[0].colorAttachmentCount = 0;
   subpass[0].pColorAttachments = NULL;
   subpass[0].pResolveAttachments = NULL;
   subpass[0].pDepthStencilAttachment = &depth_ref;
   subpass[0].preserveAttachmentCount = 0;
   subpass[0].pPreserveAttachments = NULL;
 
   // Create render pass
   VkRenderPassCreateInfo renderPassInfo;
   renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
   renderPassInfo.pNext = NULL;
   renderPassInfo.attachmentCount = 1;
   renderPassInfo.pAttachments = attachments;
   renderPassInfo.subpassCount = 1;
   renderPassInfo.pSubpasses = subpass;
   renderPassInfo.dependencyCount = 0;
   renderPassInfo.pDependencies = NULL;
   renderPassInfo.flags = 0;
 
   if(vkCreateRenderPass(device, &renderPassInfo, nullptr, &shadowmapRenderPass) != VK_SUCCESS){
        throw std::runtime_error("Failed to create shadowmap render pass");
    }
    _swapDeletionQueue.push_function([=](){vkDestroyRenderPass(device, shadowmapRenderPass, nullptr);});
}*/

void Vk::Renderer::resetScene(){
    sceneShutdownRequest = true;
}

void Vk::Renderer::setRenderablesPointer(std::vector<std::shared_ptr<RenderObject>>* renderableObjects){
    p_renderables = renderableObjects;

    //this is temporarily here
    for(auto iter : _materials){
        _materialParameters[iter.second.propertiesId].diffuse = iter.second.diffuse;
        _materialParameters[iter.second.propertiesId].specular = iter.second.specular;
        _materialParameters[iter.second.propertiesId].extra = iter.second.extra;
    }
}

void Vk::Renderer::setLightPointers(WorldLightObject* sceneLight, std::vector<WorldPointLightObject>* pointLights, std::vector<WorldSpotLightObject>* spotLights){
    p_sceneLight = sceneLight;
    p_pointLights = pointLights;
    p_spotLights = spotLights;
}

//void Vk::Renderer::allocateShadowMapImages(){
    /*
    uint32_t layerCounter = 0;
    //start with scene light
    //scene is layer 0 in shadowmapImage
    shadowmapImageViews.push_back(VkImageView());
    VkImageViewCreateInfo viewInfo = Vk::Structures::image_view_create_info(shadowmapImage, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, layerCounter, 1);
    if(vkCreateImageView(device, &viewInfo, nullptr, &shadowmapImageViews[layerCounter]) != VK_SUCCESS){
        throw std::runtime_error("Failed to create scene shadowMapImageView");
    }
    _textureDeletionQueue.push_function([=](){vkDestroyImageView(device, shadowmapImageViews[layerCounter], nullptr);});

    p_sceneLight->layer = layerCounter++;

    //then each spotlight
    for(int i = 0; i < p_spotLights->size(); i++){
        VkImageViewCreateInfo viewInfo = Vk::Structures::image_view_create_info(shadowmapImage, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, layerCounter, 1);
        if(vkCreateImageView(device, &viewInfo, nullptr, &shadowmapImageViews[layerCounter]) != VK_SUCCESS){
            throw std::runtime_error("Failed to create p_spotLights shadowMapImageView");
        }
        _textureDeletionQueue.push_function([=](){vkDestroyImageView(device, shadowmapImageViews[layerCounter], nullptr);});
        p_spotLights->at(i).layer = layerCounter++;
    }
    //then point lights, ommiting for now as we won't have any and i'd need to work out cube maps


    createShadowMapFrameBuffer();
    */
//}

//void Vk::Renderer::allocateDescriptorSetForShadowmap(){
    /*VkWriteDescriptorSet setWrite{};
    VkDescriptorImageInfo shadowmapImageInfo;
    shadowmapImageInfo.sampler = shadowmapSampler;
    shadowmapImageInfo.imageView = shadowmapImageViews.data(); //this should exist before this call
    shadowmapImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    setWrite = Vk::Structures::write_descriptorset(0, skyboxSet, 1, 
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &shadowmapImageInfo);
    queueSubmitMutex.lock();
    vkUpdateDescriptorSets(device,  1, &setWrite, 0, nullptr);
    queueSubmitMutex.unlock();*/
//}

void Vk::Renderer::allocateDescriptorSetForSkybox(){
    VkWriteDescriptorSet setWrite{};
    VkDescriptorImageInfo skyImageInfo;
    skyImageInfo.sampler = skyboxSampler;
    skyImageInfo.imageView = skyboxImageView; //this should exist before this call
    skyImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    setWrite = Vk::Structures::write_descriptorset(0, skyboxSet, 1, 
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &skyImageInfo);
    queueSubmitMutex.lock();
    vkUpdateDescriptorSets(device,  1, &setWrite, 0, nullptr);
    queueSubmitMutex.unlock();
}

void Vk::Renderer::allocateDescriptorSetForTexture(std::string materialName, std::string name){
    Material* material = getMaterial(materialName);
    if(material != nullptr){
        material->_multiTextureSets.resize(1);
        //we also allocate our multiTextureSet here but not per frame and we dont write it yet
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.pNext = nullptr;
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 2;
        allocInfo.pSetLayouts = &_multiTextureSetLayout;

        vkAllocateDescriptorSets(device, &allocInfo, &material->_multiTextureSets[0]);

        std::vector<VkWriteDescriptorSet> setWrite;
        VkDescriptorImageInfo diffImageInfo;
        diffImageInfo.sampler = diffuseSampler;
        diffImageInfo.imageView = _loadedTextures[name+"_diff"].imageView;
        diffImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorImageInfo specImageInfo;
        specImageInfo.sampler = specularSampler;
        specImageInfo.imageView = _loadedTextures[name+"_spec"].imageView;
        specImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        setWrite.push_back(Vk::Structures::write_descriptorset(0, material->_multiTextureSets[0], 1, 
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &diffImageInfo));

        setWrite.push_back(Vk::Structures::write_descriptorset(1, material->_multiTextureSets[0], 1, 
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &specImageInfo));

        queueSubmitMutex.lock();
        try{
            vkUpdateDescriptorSets(device,  setWrite.size(), setWrite.data(), 0, nullptr); // <--- seg fault here setWrite.data()
        }
        catch(std::exception& e){
            std::cout << "Failed to update descriptor sets for  Texture " + name + "\n";
            std::cout << e.what() << "\n";
        }
        queueSubmitMutex.unlock();
    }
}

void Vk::Renderer::updateObjectTranslations(){
    glm::mat4 scale;
    glm::mat4 translation;
    glm::mat4 rotation;

    for (int i = 0; i < p_renderables->size(); i++){
        scale = glm::scale(glm::mat4{ 1.0 }, p_renderables->at(i)->scale);
        translation = glm::translate(glm::mat4{ 1.0 }, p_renderables->at(i)->pos);
        rotation = p_renderables->at(i)->rot;
        p_renderables->at(i)->transformMatrix = translation * rotation * scale;
    }
}

void Vk::Renderer::setCameraData(CameraData* camData){
    p_cameraData = camData;
}

void Vk::Renderer::populateCameraData(GPUCameraData& camData){
    glm::vec3 camPos = p_cameraData->cameraPos;
    glm::mat4 view = glm::lookAt(camPos, camPos + p_cameraData->cameraFront, p_cameraData->cameraUp);
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float) swapChainExtent.height, 0.1f, 15000.0f);
    //GLM was originally designed for OpenGL, where the Y coordinate of the clip coordinates is inverted, so we simply flip it
    //view[1][1] *= -1; //<--- really trippy :)
    proj[1][1] *= -1;
    camData.projection = proj;
    camData.view = view;
	camData.viewproj = proj * view;
}

void Vk::Renderer::populateLanderCameraData(GPUCameraData& camData){
    RenderObject* lander =  p_renderables->at(2).get(); //lander is obj 2
    glm::vec3 camPos = lander->pos;
    glm::vec3 landingSitePos = glm::vec3(0,0,0); //origin for now
    glm::vec3 landerUp = glm::vec3(0,1,0); //need to get lander up, which means getting it from bullet and storing it
    //need to rotate by landerForward?
    glm::mat4 view = glm::lookAt(camPos, landingSitePos, landerUp);
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float) swapChainExtent.height, 0.1f, 15000.0f);
    proj[1][1] *= -1;
    camData.projection = proj;
    camData.view = view;
	camData.viewproj = proj * view;
}

void Vk::Renderer::mapMaterialDataToGPU(){ //not sure i need 3 of these if they were going to be read only anyway, test at some point
    for(int i = 0; i < swapChainImages.size(); i++){
        //copy material data array into position, can i do this outside of the render loop as it will be static? lets try
        char* materialData;
        vmaMapMemory(allocator, _frames[i].materialPropertiesBufferAlloc , (void**)&materialData);
        memcpy(materialData, &_materialParameters, sizeof(_materialParameters));
        vmaUnmapMemory(allocator, _frames[i].materialPropertiesBufferAlloc); 
    }
}

void Vk::Renderer::mapLightingDataToGPU(){
    for(int i = 0; i < swapChainImages.size(); i++){
        //copy current point light data array into buffer
        char* pointLightingData;
        vmaMapMemory(allocator, _frames[i].pointLightParameterBufferAlloc , (void**)&pointLightingData);
        memcpy(pointLightingData, &_pointLightParameters, sizeof(_pointLightParameters));
        vmaUnmapMemory(allocator, _frames[i].pointLightParameterBufferAlloc); 
    }
    for(int i = 0; i < swapChainImages.size(); i++){
        //copy current spot light data array into buffer
        char* spotLightingData;
        vmaMapMemory(allocator, _frames[i].spotLightParameterBufferAlloc , (void**)&spotLightingData);
        memcpy(spotLightingData, &_spotLightParameters, sizeof(_spotLightParameters));
        vmaUnmapMemory(allocator, _frames[i].spotLightParameterBufferAlloc); 
    }

    //now for the lightVP matrix uniform buffer
    for(int i = 0; i < swapChainImages.size(); i++){
        //copy current point light data array into buffer
        char* lightVPData;
        vmaMapMemory(allocator, _frames[i].lightVPBufferAlloc , (void**)&lightVPData);
        memcpy(lightVPData, &lightVPParameters, sizeof(lightVPParameters));
        vmaUnmapMemory(allocator, _frames[i].lightVPBufferAlloc); 
    }
}

//
void Vk::Renderer::updateSceneData(GPUCameraData& camData){
    glm::vec3 lightDir = glm::vec3(camData.view * glm::vec4(p_sceneLight->pos, 0));
    _sceneParameters.lightDirection = glm::vec4(lightDir, 0);
    _sceneParameters.lightAmbient = glm::vec4(p_sceneLight->ambient, 1);
    _sceneParameters.lightDiffuse = glm::vec4(p_sceneLight->diffuse, 1);
    _sceneParameters.lightSpecular = glm::vec4(p_sceneLight->specular, 1);
    // Compute the MVP matrix from the light's point of view, ortho projection, need to adjust values
    glm::mat4 projectionMatrix = glm::ortho<float>(-1000,1000,-1000,1000,-1000,1000);
    glm::mat4 viewMatrix = glm::lookAt(lightDir, glm::vec3(0,0,0), glm::vec3(0,1,0));
    glm::mat4 modelMatrix = glm::mat4(1.0);
    lightVPParameters[p_sceneLight->layer].viewproj = projectionMatrix * viewMatrix * modelMatrix;
    //this is used to render the scene from the lights POV to generate a shadow map
}

//point lights
void Vk::Renderer::updateLightingData(GPUCameraData& camData){
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

        //mvpMatrix for shadowmap, perpective projection
        glm::mat4 clip = glm::mat4(1.0f, 0.0f, 0.0f, 0.0f,
                           0.0f,-1.0f, 0.0f, 0.0f,
                           0.0f, 0.0f, 0.5f, 0.0f,
                           0.0f, 0.0f, 0.5f, 1.0f);

        glm::mat4 projectionMatrix = clip * glm::perspective<float>(glm::radians(p_spotLights->at(i).cutoffAngles.y), 1.0f,  p_spotLights->at(i).cutoffAngles.x, p_spotLights->at(i).cutoffAngles.y);
        
        glm::mat4 viewMatrix = glm::lookAt(p_spotLights->at(i).pos, p_spotLights->at(i).pos - p_spotLights->at(i).direction, glm::vec3(0,1,0));
        glm::mat4 modelMatrix = glm::mat4(1.0);
        lightVPParameters[p_sceneLight->layer].viewproj = projectionMatrix * viewMatrix * modelMatrix;
    }
}

void Vk::Renderer::setShouldDrawOffscreen(bool b){
    shouldDrawOffscreenFrame = b;
}

int i = 0;
void Vk::Renderer::drawOffscreenFrame(){
    if(i == 0){
        recordCommandBuffer_Offscreen();
        i++;
    }
    else{
        writeOffscreenImageToDisk();
        i = 0;
    }
    shouldDrawOffscreenFrame = false;
}

void Vk::Renderer::fillOffscreenImageBuffer(){
    imageHelper->copyImageToBuffer(offscreenImageBuffer, offscreenImage, OFFSCREEN_IMAGE_WIDTH, OFFSCREEN_IMAGE_HEIGHT, 1);
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
}

void Vk::Renderer::drawFrame(){
    if (shouldDrawOffscreenFrame)
        drawOffscreenFrame();

    vkWaitForFences(device, 1, &_frames[currentFrame]._renderFence, VK_TRUE, UINT64_MAX); 
    
    //so the first step is to acquire an image from the swap chain
    uint32_t imageIndex;
    //we will store the return rsult of the call VkResult and use it to check if the swap chain needs recreated if its out of date
    VkResult result = vkAcquireNextImageKHR(device,swapChain, UINT64_MAX, _frames[currentFrame]._presentSemaphore, VK_NULL_HANDLE, &imageIndex);
    if(result == VK_ERROR_OUT_OF_DATE_KHR){
        recreateSwapChain();
        return;
    }
    else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR){
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    //before we continue using the frame we should check to make sure it is not in use in the chain somewhere
    if(imagesInFlight[imageIndex] != VK_NULL_HANDLE){
        vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    //once we are clear we assign this inFlightFence to imagesInFlight so we can keep it locked until this pass is done for this image
    imagesInFlight[imageIndex] = _frames[currentFrame]._renderFence;

    //we have the imageIndex and are cleared for operations on it so it should be safe to rerecord the command buffer here
    recordCommandBuffer_Objects(imageIndex);
    recordCommandBuffer_GUI(imageIndex);

    VkSemaphore waitSemaphores[] = {_frames[currentFrame]._presentSemaphore}; 
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}; 
    VkSemaphore signalSemaphores[] = {_frames[currentFrame]._renderSemaphore};

    //Submitting the command buffer
    //queue submission and synchronization is configured through VkSubmitInfo struct
    VkCommandBuffer buffers[] = {_frames[imageIndex]._mainCommandBuffer, guiCommandBuffers[imageIndex]};
    VkSubmitInfo submitInfo = Vk::Structures::submit_info(buffers, 2, waitSemaphores, 1,
        waitStages, signalSemaphores, 1);
    
    //move our reset fences here, its best to simply reset a fence right before its used
    vkResetFences(device, 1, &_frames[currentFrame]._renderFence); //unlike semaphores, fences must be manually reset

    //submit the queue
    queueSubmitMutex.lock();
    if(vkQueueSubmit(graphicsQueue, 1 , &submitInfo, _frames[currentFrame]._renderFence) != VK_SUCCESS) { //then fails here after texture flush
        throw std::runtime_error("Failed to submit draw command buffer");
    }
    
    //Presentation
    //The last step of drawing a frame is submitting the result back to the swapchain
    VkSwapchainKHR swapChains[] = {swapChain};
    VkPresentInfoKHR presentInfo = Vk::Structures::present_info(&_frames[currentFrame]._renderSemaphore, 1, swapChains, &imageIndex);

    //now we submit the request to present an image to the swapchain. We'll add error handling later, as failure here doesnt necessarily mean we should stop
    result = vkQueuePresentKHR(presentQueue, &presentInfo); //takes ages after texture flush for some reason

    queueSubmitMutex.unlock();

    //in addition to these checks we also check member variable frameBufferResized for manual confirmation
    //must do this after vkQueuePresentKHR() call to ensure semaphores are in a consinstent state, otherwise a signalled semaphore may never be properly waited on
    //to actually manually detect resizes we use glfwSetFramebufferSizeCallback() to set a callback in initWindow()
    //if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || frameBufferResized){
    if(result == VK_ERROR_OUT_OF_DATE_KHR){ //ignoring frameBufferResized and VK_SUBOPTIMAL_KHR stops race condition causing validation errors, not ideal
        //frameBufferResized = false;
        recreateSwapChain();
    }
    else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR){
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    frameCounter++;
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    calculateFrameRate(); //just to display in the UI

    //if user has quit the scene we set resetTextureImages to true
    //then flush textures and related buffers, unset the renderables pointer
    //this could be cleaner
    if(sceneShutdownRequest){
        sceneShutdownRequest = false;
        flushSceneBuffers();
        flushTextures();
        r_mediator.application_resetScene();
        p_renderables = NULL;
    }
}

void Vk::Renderer::recordCommandBuffer_Offscreen(){
    //similar to recordCommandBuffer_Objects but calculate shadowmap for each light
    vkResetCommandBuffer(offscreenCommandBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT); 

    VkCommandBufferBeginInfo beginInfo = Vk::Structures::command_buffer_begin_info(0);
    if(vkBeginCommandBuffer(offscreenCommandBuffer, &beginInfo) != VK_SUCCESS){
        throw std::runtime_error("Failed to begin recording offscreen command buffer");
    }

    VkExtent2D extent{OFFSCREEN_IMAGE_WIDTH, OFFSCREEN_IMAGE_HEIGHT};
    
    //note that the order of clear values should be identical to the order of attachments
    //VkRenderPassBeginInfo renderPassBeginInfo = Vk::Structures::render_pass_begin_info(offscreenRenderPass, offscreenFramebuffer, {0, 0}, extent, clearValues);
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

/*void Vk::Renderer::recordCommandBuffer_ShadowMaps(int i){
    //similar to recordCommandBuffer_Objects but calculate shadowmap for each light
    vkResetCommandBuffer(_frames[i]._mainCommandBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT); 

    VkCommandBufferBeginInfo beginInfo = Vk::Structures::command_buffer_begin_info(0);
    if(vkBeginCommandBuffer(_frames[i]._mainCommandBuffer, &beginInfo) != VK_SUCCESS){
        throw std::runtime_error("Failed to begin recording command buffer in shadowpass");
    }

    VkExtent2D extent{SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT};
    
    //note that the order of clear values should be identical to the order of attachments
    VkRenderPassBeginInfo renderPassBeginInfo = Vk::Structures::render_pass_begin_info(shadowmapRenderPass, shadowmapFramebuffer, {0, 0}, extent, clearValues);

    vkCmdBeginRenderPass(_frames[i]._mainCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    if(r_mediator.application_getSceneLoaded())
    if(p_renderables != NULL && !p_renderables->empty()){   
        VkBuffer vertexBuffers[] = {vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(_frames[i]._mainCommandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(_frames[i]._mainCommandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        drawShadowMaps(i);
    }
    
    //we we can end the render pass
    vkCmdEndRenderPass(_frames[i]._mainCommandBuffer);

    //and we have finished recording, we check for errors here
    if (vkEndCommandBuffer(_frames[i]._mainCommandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record shadowmap command buffer!");
    }
}*/

void Vk::Renderer::recordCommandBuffer_Objects(int i){
    //have to reset this even if not recording it!
    vkResetCommandBuffer(_frames[i]._mainCommandBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT); 

    //must record even if scene isnt loaded to allow it to clear the background from the UI
    VkCommandBufferBeginInfo beginInfo = Vk::Structures::command_buffer_begin_info(0);
    if(vkBeginCommandBuffer(_frames[i]._mainCommandBuffer, &beginInfo) != VK_SUCCESS){
        throw std::runtime_error("Failed to begin recording command buffer");
    }

    //note that the order of clear values should be identical to the order of attachments
    VkRenderPassBeginInfo renderPassBeginInfo = Vk::Structures::render_pass_begin_info(renderPass, swapChainFramebuffers[i], {0, 0}, swapChainExtent, clearValues);

    vkCmdBeginRenderPass(_frames[i]._mainCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    if(r_mediator.application_getSceneLoaded())
    if(p_renderables != NULL && !p_renderables->empty()){   
        VkBuffer vertexBuffers[] = {vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(_frames[i]._mainCommandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(_frames[i]._mainCommandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        drawObjects(i);
    }
    
    //we we can end the render pass
    vkCmdEndRenderPass(_frames[i]._mainCommandBuffer);

    //and we have finished recording, we check for errrors here
    if (vkEndCommandBuffer(_frames[i]._mainCommandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void Vk::Renderer::recordCommandBuffer_GUI(int i){
    //now for the UI render pass
    vkResetCommandBuffer(guiCommandBuffers[i], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT); 
    
    VkCommandBufferBeginInfo beginInfo = Vk::Structures::command_buffer_begin_info(0);
    if(vkBeginCommandBuffer(guiCommandBuffers[i], &beginInfo) != VK_SUCCESS){
        throw std::runtime_error("Failed to begin recording gui command buffer");
    }

    r_mediator.ui_drawUI(); //then we will draw UI

    //note that the order of clear values should be identical to the order of attachments 
    VkRenderPassBeginInfo renderPassBeginInfo = Vk::Structures::render_pass_begin_info(guiRenderPass, guiFramebuffers[i], {0, 0}, swapChainExtent, clearValues);
    vkCmdBeginRenderPass(guiCommandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Record Imgui Draw Data and draw funcs into command buffer
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), guiCommandBuffers[i]);

    //we we can end the render pass
    vkCmdEndRenderPass(guiCommandBuffers[i]);

     //and we have finished recording, we check for errrors here
    if (vkEndCommandBuffer(guiCommandBuffers[i]) != VK_SUCCESS) {
        throw std::runtime_error("failed to record gui command buffer!");
    }
}

void Vk::Renderer::drawOffscreen(int curFrame){
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

    int renderObjectIds [2] = {1,3}; //renderables 1 and 3, these are 1 = star, 3 = asteroid, 
    //need to include 4 landing site boxes too, and not render them in main loop
    
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

/*void Vk::Renderer::drawShadowMaps(int curFrame){
     ////fill a GPU camera data struct
	GPUCameraData camData;
    populateCameraData(camData);
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
  
    for (int i = 1; i < p_renderables->size(); i++){
        RenderObject* object = p_renderables->at(i).get();
        //only bind the pipeline if it doesn't match with the already bound one
		vkCmdBindPipeline(offscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowmapPipeline);

        //offset for our scene buffer
        /*uint32_t scene_uniform_offset = pad_uniform_buffer_size(sizeof(GPUSceneData)) * frameIndex;
        //bind the descriptor set when changing pipeline
        //vkCmdBindDescriptorSets(offscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
        //    object->material->pipelineLayout, 0, 1, &_frames[curFrame].globalDescriptor, 1, &scene_uniform_offset);

        vkCmdBindDescriptorSets(offscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
            shadowmapPipelineLayout, 0, 1, &_frames[curFrame].lightVPSet, 0, nullptr);

        //object data descriptor
        vkCmdBindDescriptorSets(offscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
            shadowmapPipelineLayout, 1, 1, &_frames[curFrame].objectDescriptor, 0, nullptr);

        //add material property id for this object to push constant
		//PushConstants constants;
		//constants.matIndex = object->material->propertiesId;
        //constants.numPointLights = p_pointLights->size();
        //constants.numSpotLights = p_spotLights->size();

        //upload the mesh to the GPU via pushconstants
		//vkCmdPushConstants(offscreenCommandBuffer, object->material->pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &constants);

        vkCmdDrawIndexed(offscreenCommandBuffer, _loadedMeshes[object->meshId].indexCount, 1, _loadedMeshes[object->meshId].indexBase, 0, i); //using i as index for storage buffer in shaders
    }
}*/

void Vk::Renderer::drawObjects(int curFrame){
    ////fill a GPU camera data struct
	GPUCameraData camData;
    populateCameraData(camData);
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

    int renderObjectIds [3] = {1,2,3}; //these are 0 = skybox,  1 = star, 2 = lander, 3 = asteroid, but skybox drawn after with different call
    
    for(const int i : renderObjectIds){
    //for (int i = 1; i < p_renderables->size(); i++){
        RenderObject* object = p_renderables->at(i).get();
        //only bind the pipeline if it doesn't match with the already bound one
		if (object->material != lastMaterial) {
            if(object->material == nullptr){throw std::runtime_error("object.material is a null reference in drawObjects()");} //remove if statement in release
			vkCmdBindPipeline(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object->material->pipeline);
			lastMaterial = object->material;

            //offset for our scene buffer
            uint32_t scene_uniform_offset = pad_uniform_buffer_size(sizeof(GPUSceneData)) * frameIndex;
            //bind the descriptor set when changing pipeline
            vkCmdBindDescriptorSets(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                object->material->pipelineLayout, 0, 1, &_frames[curFrame].globalDescriptor, 1, &scene_uniform_offset);

	        //object data descriptor
        	vkCmdBindDescriptorSets(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                object->material->pipelineLayout, 1, 1, &_frames[curFrame].objectDescriptor, 0, nullptr);

             //material descriptor, we are using a dynamic uniform buffer and referencing materials by their offset with propertiesId, which just stores the int relative to the material in _materials
        	vkCmdBindDescriptorSets(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                object->material->pipelineLayout, 2, 1, &_frames[curFrame].materialSet, 0, nullptr);
            //the "firstSet" param above is 2 because in init_pipelines its described in VkDescriptorSetLayout[] in index 2!
            
            vkCmdBindDescriptorSets(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                object->material->pipelineLayout, 3, 1, &_frames[curFrame].lightSet, 0, nullptr);

            if (object->material->_multiTextureSets.size() > 0) {
                vkCmdBindDescriptorSets(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                    object->material->pipelineLayout, 4, 1, &object->material->_multiTextureSets[0], 0, nullptr);
		    }
		}

        //add material property id for this object to push constant
		PushConstants constants;
		constants.matIndex = object->material->propertiesId;
        constants.numPointLights = p_pointLights->size();
        constants.numSpotLights = p_spotLights->size();

        //upload the mesh to the GPU via pushconstants
		vkCmdPushConstants(_frames[curFrame]._mainCommandBuffer, object->material->pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &constants);

        vkCmdDrawIndexed(_frames[curFrame]._mainCommandBuffer, _loadedMeshes[object->meshId].indexCount, 1, _loadedMeshes[object->meshId].indexBase, 0, i); //using i as index for storage buffer in shaders
    }

    //draw skybox
    uint32_t scene_uniform_offset = pad_uniform_buffer_size(sizeof(GPUSceneData)) * frameIndex;
    vkCmdBindDescriptorSets(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _skyboxPipelineLayout, 0, 1, &_frames[curFrame].globalDescriptor, 1, &scene_uniform_offset);
    vkCmdBindDescriptorSets(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _skyboxPipelineLayout, 1, 1, &_frames[curFrame].objectDescriptor, 0, nullptr);
    vkCmdBindDescriptorSets(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _skyboxPipelineLayout, 2, 1, &skyboxSet, 0, NULL);
	vkCmdBindPipeline(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline);
	vkCmdDrawIndexed(_frames[curFrame]._mainCommandBuffer, _loadedMeshes[p_renderables->at(0)->meshId].indexCount, 1, _loadedMeshes[p_renderables->at(0)->meshId].indexBase, 0, 0);
}

void Vk::Renderer::calculateFrameRate(){
    double currentFrameTime = glfwGetTime();
    if(currentFrameTime - previousFrameTime >= 1.0 ){ // If last update was more than 1 sec ago
        renderStats.framerate = 1000.0/double(frameCounter-previousFrameCount);
        renderStats.fps = frameCounter-previousFrameCount;
        previousFrameTime += 1.0;
        previousFrameCount = frameCounter;
    }
}

//creates texture images by loading them from file and 
void Vk::Renderer::createTextureImages(const std::vector<TextureInfo>& TEXTURE_INFOS, const std::vector<std::string>& SKYBOX_PATHS){
    _loadedTextures.clear();
    for(TextureInfo info : TEXTURE_INFOS){
        Texture texture;
        std::cout << info.filePath.c_str() << "\n";
        imageHelper->loadTextureImage(*this, info.filePath.c_str(), texture.image, texture.alloc, texture.mipLevels);
        VkImageViewCreateInfo imageinfo = Vk::Structures::image_view_create_info(texture.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_ASPECT_COLOR_BIT, 0, texture.mipLevels, 0, 1);
        vkCreateImageView(device, &imageinfo, nullptr, &texture.imageView);  
        _loadedTextures[info.textureName] = texture;
        _textureDeletionQueue.push_function([=](){vmaDestroyImage(allocator, _loadedTextures[info.textureName].image, _loadedTextures[info.textureName].alloc);});
        _textureDeletionQueue.push_function([=](){vkDestroyImageView(device, _loadedTextures[info.textureName].imageView, nullptr);});
    }

    //load skybox textures
    char* textureData[6];
    
    int width{ 0 };
    int height{ 0 };
    int numberOfChannels{ 0 };
    
    int i{0};
    for(std::string path : SKYBOX_PATHS){
        imageHelper->simpleLoadTexture(path.c_str(), width, height, textureData[i]);
        i++;
    }
        
    //Calculate the image size and the layer size.
    const VkDeviceSize imageSize = width * height * 4 * 6; //4 since I always load my textures with an alpha channel, and multiply it by 6 because the image must have 6 layers.
    const VkDeviceSize layerSize = imageSize / 6; //This is just the size of each layer.
    
    //Staging buffer
    //we will now create a host visible staging buffer so that we can map memory and copy pixels to it
    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferAllocation;
    createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, stagingBufferAllocation);
    
    //map the memory
    char* dataMemory;
    vmaMapMemory(allocator, stagingBufferAllocation, (void**)&dataMemory);
    
    //Copy the data into the staging buffer.
    for (uint8_t i = 0; i < 6; ++i){    
        memcpy(dataMemory + (layerSize * i), textureData[i], static_cast<size_t>(layerSize));
    }
    vmaUnmapMemory(allocator, stagingBufferAllocation);

    //Copy the data into the staging buffer.
    for (uint8_t i = 0; i < 6; ++i){    
        imageHelper->simpleFreeTexture(textureData[i]);
    }

    imageHelper->createImage(width, height, 1, 6, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, skyboxImage, skyboxAllocation, VMA_MEMORY_USAGE_GPU_ONLY);

    //we have created the texture image, next step is to copy the staging buffer to the texture image. This involves 2 steps
    //we first transition the image to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    //note because the iamge was created with VK_IMAGE_LAYOUT_UNDEFINED, we specify that as the initial layout. We can do this because we dont care
    //about its contents before performing the copy operation
    imageHelper->transitionImageLayout(skyboxImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, 6);
    //then execute the buffer to image copy operation
    imageHelper->copyBufferToImage(stagingBuffer, skyboxImage, static_cast<uint32_t>(width), static_cast<uint32_t>(height), 6);

    imageHelper->transitionImageLayout(skyboxImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 6);

    vmaDestroyBuffer(allocator, stagingBuffer, stagingBufferAllocation);

    skyboxImageView = imageHelper->createImageView(skyboxImage, VK_IMAGE_VIEW_TYPE_CUBE, VK_FORMAT_R8G8B8A8_SRGB,  VK_IMAGE_ASPECT_COLOR_BIT, 1, 6);

    _textureDeletionQueue.push_function([=](){vmaDestroyImage(allocator, skyboxImage, skyboxAllocation);});
    _textureDeletionQueue.push_function([=](){vkDestroyImageView(device, skyboxImageView, nullptr);});
}

//this is where we will load the model data into vertices and indices member variables
void Vk::Renderer::loadModels(const std::vector<ModelInfo>& MODEL_INFOS){
    //because we are using an unnordered_map with a custom type, we need to define equality and hash functons in Vertex
    std::unordered_map<Vertex, uint32_t> uniqueVertices{}; //store unique vertices in here, and check for repeating verticesto push index
    allIndices.clear();
    allVertices.clear();

    for(ModelInfo info : MODEL_INFOS){
        glm::vec3 colour = glm::vec3(0,0,1);//temp colour code
        if(info.modelName == "sphere")//temp colour code
            colour = glm::vec3(1,1,0.5f);//temp colour code
        else
            colour = glm::vec3(0,0,1);//temp colour code
        populateVerticesIndices(info.filePath, uniqueVertices, colour);
        _meshes[info.modelName] = _loadedMeshes[_loadedMeshes.size()-1];
    }
    std::cout << "Vertices count for all " << allVertices.size() << "\n";
    std::cout << "Indices count for all " << allIndices.size() << "\n";

    createVertexBuffer(); //error here if load models later
    createIndexBuffer(); //error here if load models later
}

void Vk::Renderer::populateVerticesIndices(std::string path, std::unordered_map<Vertex, uint32_t>& uniqueVertices, glm::vec3 baseColour){
    Mesh mesh;
    _loadedMeshes.push_back(mesh);
    int numMeshes = _loadedMeshes.size() - 1;
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if(!tinyobj::LoadObj(&attrib, &shapes, & materials, & warn, & err, path.c_str())){
        throw std::runtime_error(warn + err);
    }

    _loadedMeshes[numMeshes].id = numMeshes; //used in identify and indexing for draw calls into storage buffer
    _loadedMeshes[numMeshes].indexBase = allIndices.size(); //start of indices will be 0 for first object

    for(const auto& shape : shapes){
        for(const auto& index : shape.mesh.indices){
            Vertex vertex{};
            //the index variable is of type tinyob::index_t which coontains vertex_index, normal_index and texcoord_index members
            //we need to use these indexes to look up the actual vertex attributes in the attrib arrays
            vertex.pos = { 
                //because attrib vertices is an  array of float instead of a format we want like glm::vec3, we multiply the index by 3
                //ie skipping xyz per vertex index. 
                attrib.vertices[3 * index.vertex_index + 0], //offset 0 is X
                attrib.vertices[3 * index.vertex_index + 1], //offset 1 is Y
                attrib.vertices[3 * index.vertex_index + 2] //offset 2 is Z
            };

            vertex.texCoord = {
                //Similarly there are two texture coordinate components per entry ie, U and V
                attrib.texcoords[2 * index.texcoord_index + 0], //offset 0 is U
                //because OBJ format assumes a coordinate system where a vertical coordinate of 0 means the bottom of the image,
                1 - attrib.texcoords[2 * index.texcoord_index + 1] //offset 1 is V
            };

            //default vertex colour
            vertex.colour = baseColour;

            vertex.normal = {
                attrib.normals[3 * index.normal_index + 0],
                attrib.normals[3 * index.normal_index + 1],
                attrib.normals[3 * index.normal_index + 2]
                
            };

            //store unique vertices in an unnordered map
            if(uniqueVertices.count(vertex) == 0){ //if vertices stored in uniqueVertices matching vertex == 0 
                uniqueVertices[vertex] = static_cast<uint32_t>(allVertices.size()); //add the count of vertices to the map matching the vertex
                allVertices.push_back(vertex); //add the vertex to vertices
            }
            allIndices.push_back(uniqueVertices[vertex]);
        }
    }
    _loadedMeshes[numMeshes].indexCount = allIndices.size() - _loadedMeshes[numMeshes].indexBase; //how many indices in this model
}

//used to pass vertices and indices to WorldState for building collision meshes from objects directly (only used for asteroid mesh)
std::vector<Vertex>& Vk::Renderer::get_allVertices(){
    return allVertices;
}
std::vector<uint32_t>& Vk::Renderer::get_allIndices(){
    return allIndices;
}

Mesh* Vk::Renderer::getLoadedMesh(const std::string& name){
	auto it = _meshes.find(name);
	if (it == _meshes.end()) {
		return nullptr;
	}
	else {
		return &(*it).second;
	}
}