#include "application.h"
#include "vk_renderer.h"
#include <stdexcept>
#include <iostream>
#include "world_physics.h"
#include "ui_input.h"
#include "world_camera.h"
#include "ui_handler.h"
#include "mediator.h"
#include "dmn_myScene.h"

int Application::run(){
    Vk::WindowHandler windowHandler;
    GLFWwindow* window; //pointer to the window, freed on cleanup() in VulkanRenderer just now

    //construct components
    Mediator mediator = Mediator();
    WorldPhysics worldPhysics = WorldPhysics(mediator);
    WorldCamera worldCamera = WorldCamera(mediator); //should make LockableCamera(objects&) class extend a CameraBase class,
    UiInput uiInput = UiInput(mediator);

    try{
        window = windowHandler.initWindow(WIDTH, HEIGHT, "LanderSimulation - Vulkan");

        Vk::Renderer engine = Vk::Renderer(window, mediator); //instanciate render engine
        UiHandler uiHandler = UiHandler(window, mediator);

        //associate objects with mediator class
        mediator.setCamera(&worldCamera);
        mediator.setUiHandler(&uiHandler);
        mediator.setPhysicsEngine(&worldPhysics);
        mediator.setRenderEngine(&engine);
        
        engine.init(); //initialise engine (note that model and texture loading might need moved out of init, when we have multiple options and a menu) (&worldPhysics)

        //this should be loaded in the main loop, once user has selected scenario
        //MyScene scene = MyScene(mediator);
        //scene.initScene(); //could be called in constructor?
        //mediator.setScene(&scene);
        //sceneLoaded = true;

        //set glfw callbacks for input and then capture the mouse 
        glfwSetWindowUserPointer(window, &uiInput);
        glfwSetKeyCallback(window, key_callback); //key_callback is defined in world_input, they must be regular functions though, not member functions because glfw is written in C and doesnt understand objects
        glfwSetCursorPosCallback(window, mouse_callback);
        glfwSetScrollCallback(window, scroll_callback);

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