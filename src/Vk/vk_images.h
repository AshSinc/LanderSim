#pragma once

#include "vk_types.h"
#include "vk_renderer.h"

namespace Vk::Images{
    void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t arrayLayers, VkSampleCountFlagBits numSamples, enum VkImageCreateFlagBits createFlags,
         VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VmaAllocation& imageAllocation);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount); //NOT COPYING MIPMAPPSSS!!!!!!!
    VkImageView createImageView(VkImage image, enum VkImageViewType viewType, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels, uint32_t arrayLayers);
    void transitionImageLayout(VkImage& image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels, uint32_t arrayLayers);
    bool loadTextureImage(Vk::Renderer& engine, const char* file, VkImage& outImage, VmaAllocation& alloc, uint32_t& mipLevels);
    void generateMipmaps(VkImage& image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

    void simpleLoadTexture(const char* file, int& width, int& height, char*& output);
    void simpleFreeTexture(void* data);
}