#include "ui_handler.h"
#include "vk_structs.h"
#include "vk_engine.h"
#include "world_state.h"
#include "world_input.h"

UiHandler::UiHandler(GLFWwindow* window, VulkanEngine* engine) : p_window{window}, p_engine{engine}{
}

void UiHandler::cleanup(){
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

UiHandler::~UiHandler(){}

void UiHandler::passEngine(VulkanEngine* engine){
    engine = engine;
}

void UiHandler::setWorld(WorldState* worldState){
    p_worldState = worldState;
}

void UiHandler::initUI(VkDevice* device, VkPhysicalDevice* pdevice, VkInstance* instance, uint32_t graphicsQueueFamily, VkQueue* graphicsQueue,
                        VkDescriptorPool* descriptorPool, uint32_t imageCount, VkFormat* swapChainImageFormat, VkCommandPool* transferCommandPool, 
                        VkExtent2D* swapChainExtent, std::vector<VkImageView>* swapChainImageViews){

    ImGui::CreateContext(); //create ImGui context
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForVulkan(p_window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    
    init_info.Instance = *instance;
    init_info.PhysicalDevice = *pdevice;
    init_info.Device = *device;
    init_info.QueueFamily = graphicsQueueFamily;
    init_info.Queue = *graphicsQueue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = *descriptorPool;
    init_info.Allocator = nullptr;
    init_info.MinImageCount = imageCount-1;
    init_info.ImageCount = imageCount;
    init_info.CheckVkResultFn = nullptr; //should pass an error handling function if(result != VK_SUCCESS) throw error etc

    //ImGui_ImplVulkan_Init() needs a renderpass so we have to create one, which means we need a few structs specified first
    VkAttachmentDescription attachmentDesc = vkStructs::attachment_description(*swapChainImageFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_LOAD, 
        VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VkAttachmentReference colourAttachmentRef = vkStructs::attachment_reference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colourAttachmentRef;

    //we are using a SubpassDependency that acts as synchronisation
    //so we tell Vulkan not to render this subpass until the subpass at index 0 has completed
    //or specifically at VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT stage, which is after colour output
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;  // or VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    //now we create the actual renderpass
    std::vector<VkAttachmentDescription> attachments; //we only have 1 but i set up the structs to take a vector so...
    attachments.push_back(attachmentDesc);

    VkRenderPassCreateInfo renderPassInfo = vkStructs::renderpass_create_info(attachments, 1, &subpass, 1, &dependency);

    if (vkCreateRenderPass(*device, &renderPassInfo, nullptr, &guiRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("Could not create ImGui render pass");
    }

    p_engine->_swapDeletionQueue.push_function([=](){vkDestroyRenderPass(*device, guiRenderPass, nullptr);});

    //then pass to ImGui implement vulkan init function
    ImGui_ImplVulkan_Init(&init_info, guiRenderPass);

    //now we copy the font texture to the gpu, we can utilize our single time command functions and transfer command pool
    VkCommandBuffer command_buffer = p_engine->beginSingleTimeCommands(*transferCommandPool);
    ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
    p_engine->endSingleTimeCommands(command_buffer, *transferCommandPool, *graphicsQueue);


    VkCommandPoolCreateInfo poolInfo = vkStructs::command_pool_create_info(graphicsQueueFamily);
    if(vkCreateCommandPool(*device, &poolInfo, nullptr, &guiCommandPool) != VK_SUCCESS){
        throw std::runtime_error("Failed to create gui command pool");
    }   
    p_engine->_swapDeletionQueue.push_function([=](){vkDestroyCommandPool(*device, guiCommandPool, nullptr);
	});

    guiCommandBuffers.resize(imageCount);

	//allocate the gui command buffer
	VkCommandBufferAllocateInfo cmdAllocInfo = vkStructs::command_buffer_allocate_info(VK_COMMAND_BUFFER_LEVEL_PRIMARY, guiCommandPool, guiCommandBuffers.size());
    
	if(vkAllocateCommandBuffers(*device, &cmdAllocInfo, guiCommandBuffers.data()) != VK_SUCCESS){
            throw std::runtime_error("Unable to allocate gui command buffers");
    }
    p_engine->_swapDeletionQueue.push_function([=](){vkFreeCommandBuffers(*device, guiCommandPool, guiCommandBuffers.size(), guiCommandBuffers.data());});

    //start by resizing the vector to hold all the framebuffers
    guiFramebuffers.resize(imageCount);
    //then iterative through create the framebuffers for each
    for(size_t i = 0; i < imageCount; i++){
        std::vector<VkImageView> attachment;
        attachment.push_back(swapChainImageViews->at(i));
        VkFramebufferCreateInfo framebufferInfo = vkStructs::framebuffer_create_info(guiRenderPass, attachment, *swapChainExtent, 1);

        if(vkCreateFramebuffer(*device, &framebufferInfo, nullptr, &guiFramebuffers[i]) != VK_SUCCESS){
            throw std::runtime_error("Failed to create gui framebuffer");
        }
        p_engine->_swapDeletionQueue.push_function([=](){vkDestroyFramebuffer(*device, guiFramebuffers[i], nullptr);});
    }
    updateUIPanelDimensions(p_window);//this needs moved to UI state transition section, must still end up being called from recreate swapchain as well
}


//this should all be moved to a UI handler
void UiHandler::drawUI(){
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    //ImGui::ShowDemoWindow();
    gui_ShowOverlay();
    //gui_ShowMainMenu();
    
    if(showEscMenu)
        gui_ShowEscMenu();

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
    if(getShowEscMenu()){
        glfwSetInputMode(p_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); //capture the cursor, for mouse movement
        showEscMenu = false;
    }
    else{
        glfwSetInputMode(p_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); //capture the cursor, for mouse movement
        showEscMenu = true;
    }
    p_worldState->changeSimSpeed(0, true);
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

        WorldState::WorldStats worldStats = p_worldState->getWorldStats();
        VulkanEngine::RenderStats renderStats = p_engine->getRenderStats();

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
    static ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    //
    ImGui::SetNextWindowPos(mainMenuPanelPos, ImGuiCond_Always, ImVec2(0,0));

    if (ImGui::Begin("MainMenu", NULL, window_flags)){
        ImGui::SetWindowSize(mainMenuPanelSize);
        ImGui::Button("Start", ImVec2(100,50));
        //ImGui::Checkbox("Use work area instead of main area", (*)true);
        //ImGui::SameLine();
        ImGui::Text("\nLander\n");     
        
       // ImGui::CheckboxFlags("ImGuiWindowFlags_NoBackground", &flags, ImGuiWindowFlags_NoBackground);
        //ImGui::CheckboxFlags("ImGuiWindowFlags_NoDecoration", &flags, ImGuiWindowFlags_NoDecoration);
        ///ImGui::Indent();
        ///ImGui::CheckboxFlags("ImGuiWindowFlags_NoTitleBar", &flags, ImGuiWindowFlags_NoTitleBar);
        ///ImGui::CheckboxFlags("ImGuiWindowFlags_NoCollapse", &flags, ImGuiWindowFlags_NoCollapse);
       // ImGui::CheckboxFlags("ImGuiWindowFlags_NoScrollbar", &flags, ImGuiWindowFlags_NoScrollbar);
        //ImGui::Unindent();
    }
    ImGui::End();
}

bool UiHandler::getShowEscMenu(){
    return showEscMenu;
}

void UiHandler::setShowEscMenu(bool b){
    showEscMenu = b;
}