#include "application.h"
#include "vk_renderer.h"
#include <stdexcept>
#include <iostream>

#include "dmn_myScene.h"

struct SceneData;

int Application::run(){
    try{
        window = windowHandler.initWindow(WIDTH, HEIGHT, "LanderSimulation - Vulkan");

        Vk::Renderer engine = Vk::Renderer(window, mediator); //instanciate render engine
        UiHandler uiHandler = UiHandler(window, mediator);

        //associate objects with mediator class
        mediator.setCamera(&worldCamera);
        mediator.setUiHandler(&uiHandler);
        mediator.setPhysicsEngine(&worldPhysics);
        mediator.setRenderEngine(&engine);
        mediator.setApplication(this);
        
        engine.init(); //initialise render engine

        //set glfw callbacks for input
        glfwSetWindowUserPointer(window, &uiInput);
        
        while (!glfwWindowShouldClose(window)){ //&& appRunning)
            glfwPollEvents(); //keep polling window events
            if(sceneLoaded){
                worldPhysics.mainLoop(); //progress world state
                worldCamera.updateFixedLookPosition(); //have to 
            }
            
            engine.drawFrame(); //render a frame
        }
        engine.cleanup();
    }
    catch (const std::exception &e){
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

void Application::loadScene(SceneData sceneData){
    scene = std::make_unique<MyScene>(mediator);
    scene->initScene(sceneData); //could be called in constructor?
    mediator.setScene(scene.get());
    bindWindowCallbacks();
    sceneLoaded = true;
}

void Application::endScene(){
    sceneLoaded = false;
    unbindWindowCallbacks();
    mediator.renderer_reset();
    mediator.physics_reset();
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