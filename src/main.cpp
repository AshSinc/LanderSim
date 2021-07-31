#include "vk_renderer.h"
#include <stdexcept>
#include <iostream>
#include "world_physics.h"
#include "ui_input.h"
#include "world_camera.h"
#include "ui_handler.h"
#include "mediator.h"

uint32_t WIDTH = 1920; //defaults, doesnt track resizing, should have a callback function here to update these from engine
uint32_t HEIGHT = 1080;

int main(){
    Vk::WindowHandler windowHandler;
    GLFWwindow* window; //pointer to the window, freed on cleanup() in VulkanRenderer just now

    //ISSUES
    //because VulkanEngine references objects in WorldState, we have to instanciate worldstate and the objects before starting the rendering engine
    //this is messy and impractical once ui and different model options are available
    //SOLUTION
    //find all worldstate references in VulkanEngine constructor and init() and remove
    //then move object instanciation to WorldState
    //this means we can then load parts of the rendering engine we need first, show a gui, and then load the simulation along with final texture loading etc for the renderer

    //construct components
    Mediator mediator = Mediator();
    WorldPhysics worldPhysics = WorldPhysics();
    WorldCamera worldCamera = WorldCamera(mediator); //should make LockableCamera(objects&) class extend a CameraBase class,
    UiInput uiInput = UiInput(mediator);

    try{

        window = windowHandler.initWindow(WIDTH, HEIGHT, "LanderSimulation - Vulkan");

        Vk::Renderer engine = Vk::Renderer(window, mediator); //instanciate render engine
        UiHandler uiHandler = UiHandler(window, &engine, mediator);

        //associate objects with mediator class
        mediator.setCamera(&worldCamera);
        mediator.setUiHandler(&uiHandler);
        mediator.setPhysicsEngine(&worldPhysics);
        mediator.setRenderEngine(&engine);
       
        //Cyclical Dependency note
        //1. Stick to OOAD principles. Don't include a header file, unless the class included is in composition relationship with the current class. Use forward declaration instead.
        //2. Design abstract classes to act as interfaces for two classes. Make the interaction of the classes through that interface.
        
        //uiHandler should be moved elsewhere?, or all classes should call init first and require pointers, and should warn on first use? or check notes above and redesign with abstract interfaces
        engine.init(); //initialise engine (note that model and texture loading might need moved out of init, when we have multiple options and a menu) (&worldPhysics)
        // ^^^^^^^^^ cycline dependency between engine and uiHandler, because of the init() function in uiHandler

        //ISSUE
        // worldState.loadCollisionMeshes(engine) is needed after loading the models in to the engine, we want to be able to do this in the main loop in respone to loading complete or a button being pressed
        //SOLUTION
        //move to main while loop. trigger in response to an event, probably need to refactor the rendering loop

        worldPhysics.loadCollisionMeshes(engine); //once models are loaded in engine worldState can reuse for creating asteroid collision mesh (will probs need moved too)

        //set glfw callbacks for input and then capture the mouse 
        glfwSetWindowUserPointer(window, &uiInput);
        glfwSetKeyCallback(window, key_callback); //key_callback is defined in world_input, they must be regular functions though, not member functions because glfw is written in C and doesnt understand objects
        glfwSetCursorPosCallback(window, mouse_callback);
        glfwSetScrollCallback(window, scroll_callback);
    
        while (!glfwWindowShouldClose(window)){ //&& appRunning)
            glfwPollEvents(); //keep polling window events
            worldPhysics.mainLoop(); //progress world state
            worldCamera.updateFixedLookPosition(); //have to 
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