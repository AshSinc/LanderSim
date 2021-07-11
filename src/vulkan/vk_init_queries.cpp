#include "vk_init_queries.h"
#include "vk_types.h"
#include <set>
#include <cstring>
#include "glfw_WindowHandler.h"
#include <vector>
#include <fstream>

VkPhysicalDeviceProperties init_query::getPhysicalDeviceProperties(VkPhysicalDevice& device){
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(device, &physicalDeviceProperties);
    return physicalDeviceProperties;
}

uint32_t init_query::geMinUniformBufferOffsetAlignment(VkPhysicalDevice& device){
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(device, &physicalDeviceProperties);

	return physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
}

//helper function that gets max usable sample count for MSAA
//the exact max number of samples can be extracted from the VkPhysicalDeviceProperties associated with our selected physical device,
//we're using a depth buffer, so we need to take into account the sample count for both colour and depth. the highest sample count
//supproted by both (&) will be the max we can support
VkSampleCountFlagBits init_query::getMaxUsableSampleCount(VkPhysicalDevice& device) {
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(device,
    &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts &
        physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
    if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
    if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

    return VK_SAMPLE_COUNT_1_BIT;
}

bool init_query::isDeviceSuitable(const VkPhysicalDevice& device, VkSurfaceKHR& surface, const std::vector<const char *>& deviceExtensions){
    //get basic device info like name type and supported vulkan version
    //VkPhysicalDeviceProperties deviceProperties;
    //vkGetPhysicalDeviceProperties(device, &deviceProperties);
    //get optional features supprted eg 64 bit floats texture compression multi viewport rendering (useful for VR)
    //VkPhysicalDeviceFeatures deviceFeatures;
    //vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    //queury queue families, check we have a valid index to a suitable queue family for us
    //findQueueFamilies and store them
    QueueFamilyIndices queueFamilyIndicesStruct = findQueueFamilies(device, surface);

    //check extensions supported
    bool extensionsSupported = checkDeviceExtensionSupport(device, deviceExtensions);

    //for this tutorial we only need to check for at least one supported image format and one supported presentation mode
    //so we can just make sure formats and presentModes are not empty
    bool swapChainAdequate = false;
    if(extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, surface);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    //get the supported features so we can check for Anisotropy support
    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    return queueFamilyIndicesStruct.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

//check which queue families are supported by the device
QueueFamilyIndices init_query::findQueueFamilies(const VkPhysicalDevice& device, VkSurfaceKHR& surface){
    QueueFamilyIndices indices;

    //as usual get number first
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    //then get list of queue families supported
    //VkQueueFamilyProperties contains some details, like type of ops supported and number of queues that can be created based ont that family
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    //we need to find at least one queue family that supports VK_QUEUE_GRAPHICS_BIT
    int i = 0;
    for (const auto& queueFamily : queueFamilies){
        if(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        //we also need to find a queue family that supports surface presentation
        //it is very likely these will end up being the same queue family
        //we could add logic to prefer a physical device that supports both for improved performance
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if(presentSupport){
            indices.presentFamily = i;
        }

        //we also want a transfer only queue family if possible. 
        //if not then we can set it to graphicsFamily as that can provide transfers too albeit less efficiently
        if((queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) && ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)) {
            indices.transferFamily = i;
        }

        if(indices.isComplete()){ //if we found what we needed we can break early
            //std::cout << indices.graphicsFamily.value() << " Found GPU family \n";
            //std::cout << indices.presentFamily.value() << " Found Present family \n";
            //std::cout << indices.transferFamily.value() << " Found Transfer family \n";
            break;
        }
        i++;
    }
    return indices;
}

SwapChainSupportDetails init_query::querySwapChainSupport(const VkPhysicalDevice& device, VkSurfaceKHR& surface){
    SwapChainSupportDetails details;
    //lets start with the easy one, basic surface capabilities returned into a single VkSurfaceCapabilitiesKHR struct
    //it required the VkPhysicalDevice and VkSurfaceKHR
    //all the support query use these parameters are they are the core components of the swap chain
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    //next we will query the supported surface formats
    //as this is a list of structs we will follow a similar pattern with 2 fuinction calls
    //first get the number
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    //make sure to resize the vector to hold all available formats
    if(formatCount != 0){
        details.formats.resize(formatCount);
        //then store the available formats in our struct in formats
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    //follow the same pattern for the presentModes
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    //make sure to resize the vector to hold all available presentModes
    if(presentModeCount != 0){
        details.presentModes.resize(presentModeCount);
        //then store the available present modes in our struct in presentModes
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }
    return details;
}

//checks for supported extensions
bool init_query::checkDeviceExtensionSupport(const VkPhysicalDevice& device, const std::vector<const char *>& deviceExtensions){
    //first get the count of extensions
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    //then get list of extensions
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    //set a list of string that starts and ends in device extension const?
    //basically a copy i think
    std::set<std::string> requiredExtensions(deviceExtensions.begin(),deviceExtensions.end());

    //std::cout << "Required Extensions \n";
    //std::set<std::string>:: iterator it;
    //for( it = requiredExtensions.begin(); it!=requiredExtensions.end(); ++it){
    //    std::string ans = *it;
    //    std::cout << ans << "\n";
    //}

    //iterate through list and remove the ones that we find, like a checklist
    //std::cout << "Available Extensions \n";
    for(const auto& extension : availableExtensions){
        //std::cout << extension.extensionName << "\n";
        requiredExtensions.erase(extension.extensionName);
    }
    //return true if we found everything
    return requiredExtensions.empty();
}

///if swapChainAdequate conditions were met then support is adequate, but there may be different modes of varying optimality
//we will use some functions to try to get an ideal value or the next best thing
VkSurfaceFormatKHR init_query::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats){
    //std::vector<VkSurfaceFormatKHR>& availableFormats
    //each VkSurfaceFormatKHR entry contains a format and colorSpace member. 
    //format member specifies colour channels and type, 
    //eg VK_FORMAT_B8G8R8A8_SRGB means we store B,G,R and alpha in that order, 8 bits each for a total of 32bits per pixel
    //colourSpace indicates if SRGB color space is supported or not using the VK_COLOR_SPACE_SRGB_NONLINEAR_KHR flag.

    //we will use SRGB if available, its more accurate perceived colours and is pretty much the standard
    //because of that we should also use an SRGB colour format, one of the most common is VK_FORMAT_B8G8R8A8_SRGB.

    //lets go through the list and see if our preferred combo is available
    for(const auto& availableFormat : availableFormats){
        if(availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR){
            return availableFormat;
        }
    }

    //if that fails we could start ranking available formats based on how "good" they are for us, but in most cases its ok to settle for the first hit
    return availableFormats[0];
}

//arguably the most important setting for the swap chain, there are 4 possible modes in Vulkan
//VK_PRESENT_MODE_IMMEDIATE_KHR: Images submitted are transferred right away, possibly resulting in tearing
//VK_PRESENT_MODE_FIFO_KHR: Display takes images from the front of queue and program inserts images at the back. if full then the program must wait.
//this is most similar to vsync, the moment the display is refreshed is known as "vertical blank"
//VK_PRESENT_MODE_FIFO_RELAXED_KHR: similar to previous mode except if the application is late and the queue was empy at the last vertical blank
//instead of waiting for the next vertical blank it will transfer it immediately, may result in visible tearing
//VK_PRESENT_MODE_MAILBOX_KHR: another variation of the second mode. Instead of blocking on a full queue, the images already queued are overwitten.
//this mode can be used to implement triple buffering, which allows significantly less latency and still avoid tearing
//only VK_PRESENT_MODE_FIFO_KHR is gauranteed to be available
VkPresentModeKHR init_query::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes){
    //lets go with triple buffering
    for(const auto& availablePresentMode : availablePresentModes){
        if(availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR){
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

//the swap extent is the resolution of the swap chain images, and its almost always the same as the res of the window we are drawing to
//the range of resolutions is defined in VkSurfaceCapabilitiesKHR structure
VkExtent2D init_query::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* pWindow){
    //Vulkan recommends matching the resolution of window by setting width and height in the currentExtent member
    //however some window managers allow us to set widfth and height in currentExtent to a special value: max value of uint32_t
    //in that case we will pick the resolution that best matches the window between minImageExtent and maxImageExtent bounds
    if(capabilities.currentExtent.width != UINT32_MAX){
        return capabilities.currentExtent;
    }
    else{
        //to handle window resizes we should take the actual size into account
        int width, height;
        glfwGetFramebufferSize(pWindow, &width, &height);
        VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
        actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.minImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.minImageExtent.height, actualExtent.height));
        return actualExtent;
    }
}

//optional, used in setting up message callback
std::vector<const char *> init_query::getRequiredExtensions(bool enableValidationLayers){
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); //VK_EXT_DEBUG_UTILS_EXTENSION_NAME macro is literal string "VK_EXT_debug_utils"
    }

    return extensions;
}

//optional, checks if all the requested layers are available
bool init_query::checkValidationLayerSupport(std::vector<const char *>  validationLayers){
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char *layerName : validationLayers)
    {
        bool layerFound = false;
        for (const auto &layerProperties : availableLayers)
        {
            if (strcmp(layerName, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }
        if (!layerFound)
        {
            return false;
        }
    }
    return true;
}

//helper function that returns a depthFormat for us to use as a depth image, utilises another helper function findSupportedFormat()
VkFormat init_query::findDepthFormat(VkPhysicalDevice& physicalDevice){
    return init_query::findSupportedFormat(physicalDevice, {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

//helper function to find supported formats for images, accepts a set of candidate formats and the tiling and features required
VkFormat init_query::findSupportedFormat(VkPhysicalDevice& physicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features){
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
        //linearTilingFeatures: Use cases that are supported with linear tiling
        //optimalTilingFeatures: Use cases that are supported with optimal tiling
        //bufferFeatures: Use cases that are supported for buffers
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } 
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    throw std::runtime_error("failed to find supported format!");
}

//another helper function that just checks if a format has a stencil component
//because we need to know which components it has when performing layout transitions on images with stencil formats
bool init_query::hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

//helper function to read a file, used for reading shaders
//read all the bytes from a file and return them in a byte array managed by vector
std::vector<char> init_query::readFile(const std::string& filename){
    //start by opening the file with 2 flags
    //ate: start reading at the end, binary: read as a binary file (avoid text transformations)
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if(!file.is_open()){
        throw std::runtime_error("Failed to open file");
    }
    //reading from the end allows us to use the read position to determine the size of the file and allocate a buffer
    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);
    //after that we can seek back to the beginning of the file and read all the bytes at once
    file.seekg(0);
    file.read(buffer.data(),fileSize);
    file.close();
    return buffer;
}