#include "ui_handler.h"
#include "vk_structures.h"
#include "mediator.h"
#include "world_stats.h"
#include <thread>
#include "vk_renderer_base.h"

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
   // ImGui::ShowDemoWindow();
    
    if(showMainMenu)
        gui_ShowMainMenu();
    else{
        if (showLoading){
            gui_ShowLoading();
        }
        else{
            gui_ShowOverlay();
            if(showEscMenu)
                gui_ShowEscMenu();
        }
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
        hideCursor();
        showEscMenu = false;
    }
    else{
        showCursor();
        showEscMenu = true;
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
        ImGui::Text("O toggles auto camera\n");
        ImGui::Text("P pauses simulation\n");
        ImGui::Text("[ ] controls time\n\n");

        WorldStats& worldStats = r_mediator.physics_getWorldStats();
        Vk::RenderStats& renderStats = r_mediator.renderer_getRenderStats();

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
            endScene();
            //std::cout << "Exit to menu\n";
        if (ImGui::Button("Exit Application", ImVec2(150,50)))
            //appRunning = false;
            std::cout << "Close App\n";
    }
    ImGui::End();
}

void UiHandler::gui_ShowMainMenu(){
    static ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f,0.5f));
    ImGui::GetStyle().WindowPadding = ImVec2(24,24);
    if (ImGui::Begin("MainMenu", NULL, window_flags)){

        ImGui::Text("\nScene Settings\n");
        ImGui::BeginGroup();
        if (ImGui::Button("Reset", ImVec2(75,25)))
            resetBtnClicked();
        ImGui::SameLine();
        if (ImGui::Button("Scenario1", ImVec2(75,25)))
            scenarioBtnClicked(1);
        ImGui::SameLine();
        if (ImGui::Button("Scenario2", ImVec2(75,25)))
            scenarioBtnClicked(2);
        ImGui::SameLine();
        if (ImGui::Button("Scenario3", ImVec2(75,25)))
            scenarioBtnClicked(3);
        ImGui::EndGroup();
        
        ImGui::BeginGroup();
        ImGui::Checkbox("Randomize Start Motion", &sceneData.RANDOMIZE_START);
        ImGui::SameLine();
        ImGui::Checkbox("Collision Course", &sceneData.LANDER_COLLISION_COURSE);
        
        ImGui::SliderFloat("Lander Distance", &sceneData.LANDER_START_DISTANCE, 50.0f, 1000.0f);
        ImGui::SliderFloat("Lander Pass Distance", &sceneData.LANDER_PASS_DISTANCE, 0.0f, 1000.0f);
        ImGui::SliderFloat("Lander Initial Speed", &sceneData.INITIAL_LANDER_SPEED, 0.0f, 50.0f);
        ImGui::SliderInt("Asteroid Scale", &sceneData.ASTEROID_SCALE, 1.0f, 8.0f);
        ImGui::SliderFloat("Gravity Multiplier", &sceneData.GRAVITATIONAL_FORCE_MULTIPLIER, 0.0f, 1.0f);
        //sceneData.GRAVITATIONAL_FORCE_MULTIPLIER*=sceneData.ASTEROID_SCALE;
        //sceneData.GRAVITATIONAL_FORCE_MULTIPLIER*=sceneData.ASTEROID_SCALE;
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup();
        if (ImGui::Button("Start", ImVec2(150,50)))
            startBtnClicked();
        if (ImGui::Button("Options", ImVec2(150,50)))
            std::cout << "Show options\n";
        if (ImGui::Button("Exit Application", ImVec2(150,50)))
            //appRunning = false;
            std::cout << "Close App\n";
        ImGui::EndGroup();
    }
    ImGui::End();
}

void UiHandler::gui_ShowLoading(){
    static ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f,0.5f));
    ImGui::GetStyle().WindowPadding = ImVec2(24,24);
    if (ImGui::Begin("Loading", NULL, window_flags)){
        ImGui::Text("Loading\n");
        ImGui::Separator();
        ImGui::Text("Please wait\n");
        loadingVariablesMutex.lock();
        ImGui::ProgressBar(loadingFraction, ImVec2(0.0f, 0.0f));
        ImGui::Text(loadingString.c_str());
        loadingVariablesMutex.unlock();
    }
    ImGui::End();
}

void UiHandler::startBtnClicked(){
    std::thread thread(&UiHandler::startScene, this); //starts running (have to pass a reference to object to allow thread to call non static member function)
    thread.detach(); //detach from main thread, runs until it ends
}

void UiHandler::resetBtnClicked(){
    loadDefaultSceneData();
}

void UiHandler::scenarioBtnClicked(int i){
    switch(i){
        case 1: sceneData = SCENARIO_1_SceneData; break;
        case 2: sceneData = SCENARIO_2_SceneData; break;
        case 3: sceneData = SCENARIO_3_SceneData; break;
    }
}

void UiHandler::loadDefaultSceneData(){
    sceneData = DEFAULT_SceneData;
}

//runs as a seperate thread
void UiHandler::startScene(){
    showMainMenu.store(false);
    showEscMenu.store(false);
    showLoading.store(true);
    r_mediator.application_loadScene(sceneData);
    showLoading.store(false);
    //hideCursor();
}

void UiHandler::endScene(){
    showEscMenu.store(false);
    showMainMenu.store(true);
    r_mediator.application_endScene();
    showCursor();
}

void UiHandler::hideCursor(){
    glfwSetInputMode(p_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); //capture the cursor, for mouse movement
    r_mediator.camera_setMouseLookActive(true);
}

void UiHandler::showCursor(){
    glfwSetInputMode(p_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); //release cursor
    r_mediator.camera_setMouseLookActive(false);
}

void UiHandler::updateLoadingProgress(float progress, std::string text){
    loadingVariablesMutex.lock();
    loadingFraction = progress; 
    loadingString = text;
    loadingVariablesMutex.unlock();
}
