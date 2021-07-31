#pragma once
#include <imgui.h> //basic gui library for drawing simple guis
#include <imgui_impl_glfw.h> //backends for glfw
#include <imgui_impl_vulkan.h> //and vulkan
//#include <imconfig.h> //empty by default, user config
#include <imstb_rectpack.h>
#include <imstb_textedit.h>
#include <imstb_truetype.h>
#include <vector>

class Mediator;

//forward declare
namespace Vk{
    class Renderer;
}
class WorldState;
class WorldInput;

class UiHandler{
    private:
    Mediator& r_mediator;
    Vk::Renderer* p_engine;
    GLFWwindow* p_window;
    WorldInput* p_worldInput;
    
    void loadScene(); //loads scene 
    void gui_ShowOverlay(); //holds configuration for statistics window
    void gui_ShowMainMenu(); //holds configuration for main fullscreen menu
    void gui_ShowEscMenu(); //holds esc menu
    void gui_ShowLoading(); //holds configuration for main loading bar
    void calculateFrameRate();
    
    
    ImVec2 statsPanelSize;
    ImVec2 mainMenuPanelSize;
    ImVec2 mainMenuPanelPos;
    ImVec2 escMenuPanelSize;
    ImVec2 escMenuPanelPos;

    bool showEscMenu = false;
    public:
    void toggleMenu();
    void initUI();
    void updateUIPanelDimensions(GLFWwindow* window);
    VkRenderPass guiRenderPass; //handle to gui render pass
    VkCommandPool guiCommandPool;
    std::vector<VkCommandBuffer> guiCommandBuffers;
    std::vector<VkFramebuffer> guiFramebuffers; //holds a framebuffer foreach VkImage in the swapchain
    void initUI(VkDevice* device, VkPhysicalDevice* pdevice, VkInstance* instance, uint32_t graphicsQueueFamily, 
                VkQueue* graphicsQueue, VkDescriptorPool* descriptorPool, uint32_t imageCount, 
                VkFormat* swapChainImageFormat, VkCommandPool* transferCommandPool, 
                VkExtent2D* swapChainExtent, std::vector<VkImageView>* swapChainImageViews);
    void drawUI(); //draw UI

    void cleanup();

    UiHandler(GLFWwindow* window, Vk::Renderer* renderer, Mediator& mediator);
    ~UiHandler();
};