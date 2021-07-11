#pragma once
#include <vk_engine.h>
#include <imgui.h> //basic gui library for drawing simple guis
#include <imgui_impl_glfw.h> //backends for glfw
#include <imgui_impl_vulkan.h> //and vulkan
#include <imconfig.h> //empty by default, user config
#include <imstb_rectpack.h>
#include <imstb_textedit.h>
#include <imstb_truetype.h>

class VulkanImGui {
public:
    VulkanImGui(VulkanEngine* vulkanEngine);
    ~VulkanImGui();
private:
    // Vulkan resources for rendering the UI
	VkSampler sampler;
    VkBuffer vertexBuffer; //holds the vertex buffer handle
    VkBuffer indexBuffer; //holds the index buffer handle (for specifying vertices by index in the vertexBuffer)
    VmaAllocation vertexBufferAllocation; //stores the handle to the assigned memory on the GPU 
    VmaAllocation indexBufferAllocation; //stores the handle to the assigned memory on the GPU 
	int32_t vertexCount = 0;
	int32_t indexCount = 0;
	VkDeviceMemory fontMemory = VK_NULL_HANDLE;
	VkImage fontImage = VK_NULL_HANDLE;
	VkImageView fontView = VK_NULL_HANDLE;
	VkPipelineCache pipelineCache;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	VkDescriptorPool descriptorPool;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSet descriptorSet;
    VkDevice* device;
	VulkanEngine* vulkanEngine;
};