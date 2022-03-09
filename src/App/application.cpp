#include "application.h"
#include "vk_renderer_offscreen.h"
#include <stdexcept>
#include <iostream>


struct SceneData;

int Application::run(){
    try{
        
        window = windowHandler.initWindow(WIDTH, HEIGHT, "LanderSimulation - Vulkan");

        //MAJOR ISSUE - renderer crashing on second optics image render trying to flush command buffer if monitor is on 60hz rather than 120hz!
        //should investigate before submision
        Vk::OffscreenRenderer renderer = Vk::OffscreenRenderer(window, mediator); //instanciate render engine
        UiHandler uiHandler = UiHandler(window, mediator);

        //associate objects with mediator class
        mediator.setCamera(&worldCamera);
        mediator.setUiHandler(&uiHandler);
        mediator.setPhysicsEngine(&worldPhysics);
        mediator.setRenderEngine(&renderer);
        mediator.setApplication(this);

        if(Service::OUTPUT_TEXT){
            writer = Service::Writer();
            mediator.setWriter(&writer);
            writer.clearOutputFolders();
            writer.openFiles();
        }

        renderer.init(); //initialise render engine
        uiHandler.init();

        //set glfw callbacks for input
        glfwSetWindowUserPointer(window, &uiInput);
        
        while (!glfwWindowShouldClose(window)){ //&& appRunning)
            glfwPollEvents(); //keep polling window events
            if(sceneLoaded){
                worldPhysics.mainLoop(); //progress world state
                worldCamera.updateFixedLookPosition(); //have to 
            }
            
            renderer.drawFrame(); //render a frame
        }
        renderer.cleanup();
    }
    catch (const std::exception &e){
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

void Application::loadScene(SceneData sceneData){
    scene = std::make_unique<MyScene>(mediator);
    mediator.setScene(scene.get());
    scene->initScene(sceneData);
    worldCamera.init();
    bindWindowCallbacks();
    sceneLoaded = true;
}

void Application::endScene(){
    sceneLoaded = false;
    unbindWindowCallbacks();
    mediator.renderer_resetScene();
    mediator.physics_reset();
    if(Service::OUTPUT_TEXT){
        writer.closeFiles();
    }
}

void Application::resetScene(){
    scene.reset();
    mediator.setScene(nullptr);
}

void Application::bindWindowCallbacks(){
    glfwSetKeyCallback(window, key_callback); //key_callback is defined in uiInput, they must be regular functions though, not member functions because glfw is written in C and doesnt understand objects
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
}

void Application::unbindWindowCallbacks(){
    glfwSetKeyCallback(window, nullptr);
    glfwSetCursorPosCallback(window, nullptr);
    glfwSetScrollCallback(window, nullptr);
}