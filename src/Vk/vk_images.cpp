#define STB_IMAGE_IMPLEMENTATION
#include "vk_images.h"
#include "vk_structures.h"
#include <stb_image.h>
#include <iostream>

void Vk::ImageHelper::createImage(uint32_t width, uint32_t height,  uint32_t mipLevels, uint32_t arrayLayers, VkSampleCountFlagBits numSamples, enum VkImageCreateFlagBits createFlags, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties, VkImage& image, VmaAllocation& imageAllocation){

    uint32_t queueFamilyIndices[] = {p_renderer->queueFamilyIndicesStruct.graphicsFamily.value()};
    VkImageCreateInfo imageInfo = Vk::Structures::image_create_info(VK_IMAGE_TYPE_2D, width, height, 1, mipLevels, arrayLayers, format, tiling, VK_IMAGE_LAYOUT_UNDEFINED, usage,
        numSamples, createFlags, VK_SHARING_MODE_EXCLUSIVE, 1, queueFamilyIndices);

    VmaAllocationCreateInfo allocInfo = Vk::Structures::vma_allocation_create_info(VMA_MEMORY_USAGE_GPU_ONLY);

    if (vmaCreateImage(p_renderer->allocator, &imageInfo, &allocInfo, &image, &imageAllocation, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }
}

//helper function that creates and returns a VkImageView
//Used for Texture creation and swap chain, more coming
VkImageView Vk::ImageHelper::createImageView(VkImage image, enum VkImageViewType viewType, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels, uint32_t arrayLayers){
    //code is the same as createImageViews, except for the format and the image
    VkImageViewCreateInfo viewInfo = Vk::Structures::image_view_create_info(image, viewType, format, aspectFlags, 0, mipLevels, 0, arrayLayers);

    VkImageView imageView;
    if(vkCreateImageView(p_renderer->device, &viewInfo, nullptr, &imageView) != VK_SUCCESS){
        throw std::runtime_error("Failed to create loadModtexture image view");
    }
    return imageView;
}

void Vk::ImageHelper::transitionImageLayout(VkImage& image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels, uint32_t arrayLayers) {
    VkCommandBuffer commandBuffer = p_renderer->beginSingleTimeCommands(p_renderer->transferCommandPool); //was cpool
    //one of the most common ways to perform layout transitions in using an Image Memory Barrier. A pipeline barrier like that is generally
    //used to sycnhronize access to resources. Like ensuring that a write to a buffer compeletes before reading from it. But it can also be used
    //to transition image layouts and transfer queue family ownership when VK_SHARING_MODE_EXCLUSIVE is used. (Interesting!)
    //There is an equivalent Buffer Memory Barrier to do this for buffers

    VkImageMemoryBarrier barrier = Vk::Structures::image_memory_barrier(image, VK_IMAGE_ASPECT_COLOR_BIT, 0, arrayLayers, mipLevels);
    //first two fields are layout. can specify VK_IMAGE_LAYOUT_UNDEFINED for oldLayout if we dont care about existing contents
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;

    //barriers are primarily used for synchronization purposes so you must specify which types of operations that involve the resource msut happen
    //before the barrier, and which operations involving the resource that msut be must waited on. We need to do that despite already using
    //a vkQueueWaitIdle to manually synchronize. The right values depend on the old and new layout

    //there are two transitions we need to handle. 
    //Undefined > Transfer Destination, transfer writes that dont need to wait on anything 
    //Transfer Destination > Shader reading, shader reads should wait on transfer writes, specifically shader reads in the fragment shader
    //because that is where we will use our texture
    //so we set up a conditional
    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;
    
    if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL){
        //The allowed values are specified https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#synchronization-access-types-supported
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        //we can see that VK_ACCESS_TRANSFER_WRITE_BIT requires the Pipeline_Stage. Since these writes dont have to wait on anything
        //we can specify and empty src access mask and VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT for the pre barrier operations to go in early
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if(oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL){
        //because the image will be written in the same pipeline stage and subsequently read by the fragment shader
        //we wait on VK_ACCESS_TRANSFER_WRITE_BIT as the source, then output VK_ACCESS_SHADER_READ_BIT in VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else{
        throw std::runtime_error("Unsupported layout transition");
    }
    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    //Vk::Renderer::endSingleTimeCommands(commandBuffer, Vk::Renderer::transferCommandPool, Vk::Renderer::graphicsQueue);
    p_renderer->endSingleTimeCommands(commandBuffer, p_renderer->transferCommandPool, p_renderer->graphicsQueue);
    
}

//helper function to record a copy buffer to image command
void Vk::ImageHelper::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount){ //copy buffer isnt copying mipmaps //NOT COPYING MIPMAPPSSS!!!!!!!
    VkCommandBuffer commandBuffer = p_renderer->beginSingleTimeCommands(p_renderer->transferCommandPool);

    //just like buffer copies you need to specify which part of the buffer is going to be copied to which part of the image, using VkBufferImageCopy structs

    VkBufferImageCopy region = Vk::Structures::buffer_image_copy(0, width, height, VK_IMAGE_ASPECT_COLOR_BIT, 0 , 0 , layerCount);
    //specifies how pixels are laid out in memory, eg you could have some padding bytes between rows of the image, 0 for both means the pixels are
    //simply tightly packed like they are in our case. 
    //imageSubresource, imageOffset and imageExtent fields indicate to which part of the image we want to copy the pixels.

    //Buffer to image copy operations are enqueued using the vkCmdCopyBufferToImage function:
    //fourth param indicates which layout the image is currently using. We are assuming the image has already been transitioned to the layout
    //that is optimal for copying pixels to. Right now we are only copying one chunk of pixels to the whole image, but its possible to specify 
    //an array of VkBufferImageCopy to perform many different copies from this buffer to the image in one operation
    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    p_renderer->endSingleTimeCommands(commandBuffer, p_renderer->transferCommandPool, p_renderer->graphicsQueue);
}

//used in manually loading a configuring a skybox
void Vk::ImageHelper::simpleLoadTexture(const char* file, int& width, int& height, char*& output){
    int texChannels;
    output = reinterpret_cast<char*>(stbi_load(file, &width, &height, &texChannels, STBI_rgb_alpha));
}
//used in manually freeing skybox
void Vk::ImageHelper::simpleFreeTexture(void* data){
    stbi_image_free(data); //dont forget to clean up the original pixel array
}

bool Vk::ImageHelper::loadTextureImage(Vk::Renderer& engine, const char* file, VkImage& outImage, VmaAllocation& alloc, uint32_t& mipLevels){

    int texWidth, texHeight, texChannels;
    //stbi_load takes the file path and number of channels to load. 
    //STBI_rgb_alpha forces it to be loaded with an alpha channels, even if it doesnt have one
    //the middle three parameters are outputs for width height and actual number of channels for the image
    //the pointer that is returned is the first element in an array of pixel values
    //the pixels are laid out row by row with 4 bytes per pixel in the case of STBI_rgb_alpha for a total of texWidth * texHeight * 4 values
    stbi_uc* pixels = stbi_load(file, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4; //4 bytes per pixel and width * height number of pixels is the space we need in bytes

    if(!pixels){
        throw std::runtime_error("Failed to load texture image");
    }

    //Mip mapping, we need to get the number of Mip levels based on the size of the image
    //max() selects the largest dimension, log2() calculates how many times that dimension can be divided by 2
    //then floor() nadles cases where the largest dimension is not a power of 2. We then +1 for the original mip level
    mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth,texHeight)))) + 1;

    //Staging buffer
    //we will now create a host visible staging buffer so that we can map memory and copy pixels to it
    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferAllocation;
    //engine.createBufferVMA(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, stagingBufferAllocation);
    p_renderer->createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, stagingBufferAllocation);

    //then we can copy the pixel data into the CPU visible buffer
    void* dataMemory;
    vmaMapMemory(p_renderer->allocator, stagingBufferAllocation, &dataMemory);
    memcpy(dataMemory, pixels, static_cast<size_t>(imageSize));
    vmaUnmapMemory(p_renderer->allocator, stagingBufferAllocation);

    stbi_image_free(pixels); //dont forget to clean up the original pixel array

    //because we are going to VkCmdBlit the image (for mip mapping to get the data for each mip level from the base texture)
    //VkCmdBlit vkCmdBlitImage is considered a transfer operation so we need to tell Vulkan we intend to use the texture image as both the surce and
    //the destination for a transfer. We simply add VK_IMAGE_USAGE_TRANSFER_SRC_BIT to indicate this
    //pass details to createImageVMA to fill textureImage and textureImageAllocation handles
    createImage(texWidth, texHeight, mipLevels, 1, VK_SAMPLE_COUNT_1_BIT, (VkImageCreateFlagBits)0, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, outImage, alloc);

    //we have created the texture image, next step is to copy the staging buffer to the texture image. This involves 2 steps
    //we first transition the image to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    //note because the iamge was created with VK_IMAGE_LAYOUT_UNDEFINED, we specify that as the initial layout. We can do this because we dont care
    //about its contents before performing the copy operation
    transitionImageLayout(outImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels, 1);
    //then execute the buffer to image copy operation
    copyBufferToImage(stagingBuffer, outImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1);

    vmaDestroyBuffer(p_renderer->allocator, stagingBuffer, stagingBufferAllocation);
    //to be able to start sampling from the texture image in the shader, we need one last transition to prepare it for shader access:
    //this transitions the iamge to a Shader Read Only Optimal layout after the transfer (which needed a VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    //transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels);
    
    //Generating Mipmaps
    //because we will use vkCmdBlitImage which like most image functions depends on the layout of the iamge
    //for optimal performance the source iamge should be VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL and dest should be VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    //Vulkan allows us to transition each mip level of an image independently. Each blit will only deal with 2 levels at a time so we
    //can transition each level into the optimal layout between blit commands. transitionImageLayout() only performs layout transitions on the 
    //entire image, so we will need to add a few more pipeline barrier commands using our helper before we are done with the image
    generateMipmaps(outImage, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, mipLevels);
    return true;
} 

//helper function that will loop through a given image and generate each mip image to the number of mip levels
//Note it is uncommon practice to generate mipmap levels at runtime, usually they are pregenerated and stored in the texture file alongside
//the base level to improve loading speed. Implemementing resizing in software and loading multiple levels from a file is left as an excersize
//we will come back to this 
void Vk::ImageHelper::generateMipmaps(VkImage& image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels){
    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(p_renderer->physicalDevice, imageFormat, &formatProperties);
    if(!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("Texture image format does not support linear blitting");
        //if that fails there are two alternatives, we could implement a fucntion that searches common texture image formats for one
        //that does support linear blitting. or we could use a library like stb_image_resize to do the blitting
    }
    
    VkCommandBuffer commandBuffer = p_renderer->beginSingleTimeCommands(p_renderer->transferCommandPool); //was cpool

    VkImageMemoryBarrier barrier = Vk::Structures::image_memory_barrier(image, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 1);
    //we will make several transitions so we'll reuse this VkImageMemoryBarrier struct and only change whats needed in the loop

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    //this loop will record each of the VkCmdBlitImage commands
    for (uint32_t i = 1; i < mipLevels; i++){ //loop starts at 1, because we already have mip 0, its the original image
        //first we transition level i - 1 to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        //this will wait for i -1 to be filled, either from previous blit command or from vkCmdCopyBufferToImage.
        //the current blit command will wait on this transition
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        //this sets up a pipeline barrier to wait on the configured barrier above,
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

        //next we specify the regions that will be used in the blit operation, the source mip level is i - 1 and the destination mip level is i
        //the two elements of the srcOffsets array determine the 3D region that data will be blitted from. dstOffsets determines the region
        //data will be blitted to. X and Y dimensions of dstOffsets[1] are divided by 2 since each mip level is half the size of previous
        //z dimension of dstOffsets and srcOffsets must be 1 since these are 2 images
        VkImageBlit blit = Vk::Structures::image_blit(mipWidth, mipHeight, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT, i);

        //now we are set up we record the blit command
        //note textureImage is used for both source and destination. this is because we are blitting between different levels of the same image
        //the source mip level was just transitioned to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL and the dest level is still VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        //from createTexture image. The last param allows us to specifya  VK_FILTER to use in the blit. 
        //We use VK_FILTER_LINEAR to enable interpolation as before
        vkCmdBlitImage(commandBuffer, 
            image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
            1, &blit, VK_FILTER_LINEAR);

        //then we need to transition the layout of mip level i - 1 to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        //wait on this 
        vkCmdPipelineBarrier(commandBuffer,VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

        //then we divide the dimensions by 2, ready for the next loop, ensuring it never becomes 0 with the if statements
        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    //before we end the command buffer, we insert one more pipeline barrier. This barrier transitions the last mip level from 
    //VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL. this wasnt handled int he loop since the
    //last blit level is never blitted from
    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    p_renderer->endSingleTimeCommands(commandBuffer, p_renderer->transferCommandPool, p_renderer->graphicsQueue); //transient queue pool cause errors was cpool
}
