#pragma once
#include <imgui.h> //basic gui library for drawing simple guis
#include <imgui_impl_glfw.h> //backends for glfw
#include <imgui_impl_vulkan.h> //and vulkan
#include <imconfig.h> //empty by default, user config
#include <imstb_rectpack.h>
#include <imstb_textedit.h>
#include <imstb_truetype.h>
#include <vector>
#include "vk_engine.h"
//#include <GLFW/glfw3.h>

class UiHandler{
    private:
    VulkanEngine* engine;
    GLFWwindow* window;
    
    void loadScene(); //loads scene 
    void gui_ShowOverlay(); //holds configuration for statistics window
    void gui_ShowMainMenu(); //holds configuration for main fullscreen menu
    void gui_ShowEscMenu(); //holds esc menu
    void gui_ShowLoading(); //holds configuration for main loading bar
    void calculateFrameRate();
    void updateUIPanelDimensions(GLFWwindow* window);
    

    ImVec2 statsPanelSize;
    ImVec2 mainMenuPanelSize;
    ImVec2 mainMenuPanelPos;
    ImVec2 escMenuPanelSize;
    ImVec2 escMenuPanelPos;

    public:
    static bool showEscMenu;
    VkRenderPass guiRenderPass; //handle to gui render pass
    VkCommandPool guiCommandPool;
    std::vector<VkCommandBuffer> guiCommandBuffers;
    std::vector<VkFramebuffer> guiFramebuffers; //holds a framebuffer foreach VkImage in the swapchain
    void initUI(GLFWwindow* window, VkDevice* device, VkPhysicalDevice* pdevice, VkInstance* instance, uint32_t graphicsQueueFamily, VkQueue* graphicsQueue,
                        VkDescriptorPool* descriptorPool, uint32_t imageCount, VkFormat* swapChainImageFormat, VkCommandPool* transferCommandPool, 
                        VkExtent2D* swapChainExtent, std::vector<VkImageView>* swapChainImageViews);
    void updateUIPanelDimensions();
    void drawUI(); //draw UI
    void passEngine(VulkanEngine* engine);
    void cleanup();
    //UiHandler();
    UiHandler(VulkanEngine* engine, GLFWwindow* window);
    ~UiHandler();
};