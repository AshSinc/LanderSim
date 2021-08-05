#include "ui_handler.h"
#include "vk_structs.h"
//#include "vk_renderer.h"
#include "mediator.h"
#include "world_stats.h"

UiHandler::UiHandler(GLFWwindow* window, Mediator& mediator) : p_window{window}, r_mediator{mediator}{
}

void UiHandler::cleanup(){
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

UiHandler::~UiHandler(){}

//this should all be moved to a UI handler
void UiHandler::drawUI(){
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    //ImGui::ShowDemoWindow();
    
    if(showMainMenu)
        gui_ShowMainMenu();
    else{
        gui_ShowOverlay();
        if(showEscMenu)
            gui_ShowEscMenu();
    }

    ImGui::Render();
}

void UiHandler::updateUIPanelDimensions(GLFWwindow* window){
    //this needs moved to UI state transition section, must still end up being called from recreate swapchain as well
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    mainMenuPanelSize = ImVec2(width/2, height/2);
    mainMenuPanelPos = ImVec2(mainMenuPanelSize.x/2, mainMenuPanelSize.y/2);
    escMenuPanelSize =  ImVec2(width/6, height/4);
    escMenuPanelPos = ImVec2(escMenuPanelSize.x/2, escMenuPanelSize.y/2);
    statsPanelSize = ImVec2(width/8, height);
}

//toggles menu on and off, should be moved to a UI handler
void UiHandler::toggleMenu(){
    if(showEscMenu){
        glfwSetInputMode(p_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); //capture the cursor, for mouse movement
        showEscMenu = false;
        r_mediator.camera_setMouseLookActive(true);
    }
    else{
        glfwSetInputMode(p_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); //capture the cursor, for mouse movement
        showEscMenu = true;
        r_mediator.camera_setMouseLookActive(false);
    }
    r_mediator.physics_changeSimSpeed(0, true);
}

void UiHandler::gui_ShowOverlay(){
    static int corner = 0;
    ImGuiIO& io = ImGui::GetIO(); //ImGuiWindowFlags_AlwaysAutoResize
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
    
    const float PAD = 10.0f;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
  
    ImVec2 window_pos;
    window_pos.x = work_pos.x + PAD;
    window_pos.y = work_pos.y + PAD;
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, ImVec2(0,0));
    ImGui::SetNextWindowSize(statsPanelSize, ImGuiCond_Always);
    
    if (ImGui::Begin("Stats", NULL, window_flags)){       
        ImGui::Text("Controls\n");
        ImGui::Separator();
        ImGui::Text("ESC toggles menu\n");
        ImGui::Text("SPACE cycles view focus\n");
        ImGui::Text("P pauses simulation\n");
        ImGui::Text("[ ] controls time\n\n");

        WorldStats& worldStats = r_mediator.physics_getWorldStats();
        Vk::Renderer::RenderStats& renderStats = r_mediator.renderer_getRenderStats();

        ImGui::Text("\nEngine\n");
        ImGui::Separator();
        ImGui::Text("Framerate: %.1f ms\n", renderStats.framerate);
        ImGui::Text("FPS: %.1f fps\n", renderStats.fps);
        ImGui::Text("Tickrate: %f\n", 0.0f);
        ImGui::Text("Simulation Speed: %.1f x\n\n", worldStats.timeStepMultiplier);

        ImGui::Text("\nLander\n");     
        ImGui::Separator();
        ImGui::Text("Velocity: %f m/s\n", worldStats.landerVelocity);
        ImGui::Text("Rotation: %f m/s\n", 0.0f);
        ImGui::Text("Grav Force: %f N\n", worldStats.gravitationalForce);
        ImGui::Text("Last Impact: %f N\n", worldStats.lastImpactForce);
        ImGui::Text("Largest Impact: %f N\n\n", worldStats.largestImpactForce);
    }
    ImGui::End();
}

void UiHandler::gui_ShowEscMenu(){
    static ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f,0.5f));
    ImGui::GetStyle().WindowPadding = ImVec2(24,24);
    if (ImGui::Begin("EscMenu", NULL, window_flags)){
        if (ImGui::Button("Return to Sim", ImVec2(150,50)))
            toggleMenu();
        if (ImGui::Button("Options", ImVec2(150,50)))
            std::cout << "Show options\n";
        if (ImGui::Button("Exit to Menu", ImVec2(150,50)))
            std::cout << "Exit to menu\n";
        if (ImGui::Button("Exit Application", ImVec2(150,50)))
            //appRunning = false;
            std::cout << "Close App\n";
    }
    ImGui::End();
}

void UiHandler::gui_ShowMainMenu(){
    static ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f,0.5f));
    ImGui::GetStyle().WindowPadding = ImVec2(24,24);
    if (ImGui::Begin("MainMenu", NULL, window_flags)){
        if (ImGui::Button("Return to Sim", ImVec2(150,50)))
            toggleMenu();
        if (ImGui::Button("Options", ImVec2(150,50)))
            std::cout << "Show options\n";
        if (ImGui::Button("Exit to Menu", ImVec2(150,50)))
            std::cout << "Exit to menu\n";
        if (ImGui::Button("Exit Application", ImVec2(150,50)))
            //appRunning = false;
            std::cout << "Close App\n";
    }
    ImGui::End();
}