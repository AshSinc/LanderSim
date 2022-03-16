#pragma once

#include "imgui.h" //basic gui library for drawing simple guis
#include "imgui_impl_vulkan.h" //and vulkan
#include "imgui_impl_glfw.h" //backends for glfw
#include "imstb_rectpack.h"
#include "imstb_textedit.h"
#include "imstb_truetype.h"

#include <vector>
#include <atomic>
#include <string>
#include <mutex>
#include "data_scene.h"
#include "lander_navstruct.h"
#include <chrono>

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
    GLFWwindow* p_window;
    WorldInput* p_worldInput;

    SceneData sceneData;
    const SceneData DEFAULT_SceneData;
    const ScenarioData_Scenario1 SCENARIO_1_SceneData;
    const ScenarioData_Scenario2 SCENARIO_2_SceneData;
    const ScenarioData_Scenario3 SCENARIO_3_SceneData;

    LandingSiteData landingSiteData;
    const LandingSiteData DEFAULT_LandingSiteData;
    const LandingSiteData_1 SCENARIO_1_LandingSiteData;
    const LandingSiteData_2 SCENARIO_2_LandingSiteData;
    //const ScenarioData_Scenario3 SCENARIO_3_LandingSiteData;

    std::vector<ImTextureID> opticsTextures;
    std::vector<ImTextureID> detectionTextures;
    ImTextureID matchTexture;

    void startBtnClicked();
    void resetBtnClicked();
    void startScene();
    void endScene();

    void gui_ShowOverlay(); //holds configuration for statistics window
    void gui_ShowMainMenu(); //holds configuration for main fullscreen menu
    void gui_ShowEscMenu(); //holds esc menu
    void gui_ShowLoading(); //holds configuration for main loading bar
    void gui_ShowOptics();
    void gui_ShowFlightParams();
    void updateFlightParams();
    void calculateFrameRate();
    void loadDefaultSceneData();
    void scenarioBtnClicked(int i);

    void clearBoost(int axis);

    ImVec2 statsPanelSize;
    ImVec2 mainMenuPanelSize;
    ImVec2 mainMenuPanelPos;
    ImVec2 escMenuPanelSize;
    ImVec2 escMenuPanelPos;
    ImVec2 opticsWindowSize;
    ImVec2 opticsWindowPos;
    ImVec2 flightWindowSize;
    ImVec2 flightWindowPos;

    std::atomic<bool> showEscMenu = false;
    std::atomic<bool> showMainMenu = true;
    std::atomic<bool> showLoading = false;

    std::atomic<float> loadingFraction = 0;

    std::string loadingString;

    void hideCursor();
    void showCursor();

    std::mutex loadingVariablesMutex;

    bool currentboost[16] = {false}; //all will be initialised to false because default value
    float boostTimerX = 0.0f;
    float boostTimerY = 0.0f;
    float boostTimerZ = 0.0f;

    std::chrono::time_point<std::chrono::system_clock> lastTick;

    public:
    void updateLoadingProgress(float progress, std::string text);
    void submitBoostCommand(LanderBoostCommand boost);
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

    UiHandler(GLFWwindow* window, Mediator& mediator);
    ~UiHandler();
    void init();
};