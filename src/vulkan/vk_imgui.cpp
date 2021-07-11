#include "vk_imgui.h"

//constructor, gets the vulkanEngine pointer passed to it, and gets device pointer from there
VulkanImGui::VulkanImGui(VulkanEngine* vulkanEngine) : vulkanEngine(vulkanEngine){
    device = &vulkanEngine->device;
    ImGui::CreateContext(); //create ImGui context
}

VulkanImGui::~VulkanImGui(){
    ImGui::DestroyContext();
    // Release all Vulkan resources required for rendering imGui
    vmaDestroyBuffer(vulkanEngine->allocator, vertexBuffer, vertexBufferAllocation);
    vmaDestroyBuffer(vulkanEngine->allocator, indexBuffer, indexBufferAllocation);
    vkDestroyImage(*device, fontImage, nullptr);
    vkDestroyImageView(*device, fontView, nullptr);
    vkFreeMemory(*device, fontMemory, nullptr);
    vkDestroySampler(*device, sampler, nullptr);
    vkDestroyPipelineCache(*device, pipelineCache, nullptr);
    vkDestroyPipeline(*device, pipeline, nullptr);
    vkDestroyPipelineLayout(*device, pipelineLayout, nullptr);
    vkDestroyDescriptorPool(*device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(*device, descriptorSetLayout, nullptr);
}