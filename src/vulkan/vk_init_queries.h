#pragma once

#include "vk_types.h"
#include "glfw_WindowHandler.h"

namespace init_query{

    VkPhysicalDeviceProperties getPhysicalDeviceProperties(VkPhysicalDevice& device);

    VkSampleCountFlagBits getMaxUsableSampleCount(VkPhysicalDevice& device);

    uint32_t geMinUniformBufferOffsetAlignment(VkPhysicalDevice& device);

    bool isDeviceSuitable(const VkPhysicalDevice& device, VkSurfaceKHR& surface, const std::vector<const char *>& deviceExtensions);

    QueueFamilyIndices findQueueFamilies(const VkPhysicalDevice& device, VkSurfaceKHR& surface);

    SwapChainSupportDetails querySwapChainSupport(const VkPhysicalDevice& device, VkSurfaceKHR& surface);

    bool checkDeviceExtensionSupport(const VkPhysicalDevice& device, const std::vector<const char *>& deviceExtensions);

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* pWindow);

    std::vector<const char *> getRequiredExtensions(bool enableValidationLayers);

    bool checkValidationLayerSupport(std::vector<const char *>  validationLayers);

    VkFormat findDepthFormat(VkPhysicalDevice& physicalDevice);

    VkFormat findSupportedFormat(VkPhysicalDevice& physicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    bool hasStencilComponent(VkFormat format);

    std::vector<char> readFile(const std::string& filename);

}