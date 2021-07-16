#include "vk_engine.h"
#include <stdexcept>
#include <iostream>
#include "world_state.h"
#include "world_input.h"

uint32_t WIDTH = 1920; //defaults, doesnt track resizing, should have a callback function here to update these from engine
uint32_t HEIGHT = 1080;
//extern bool appRunning = true;

int main(){
    WindowHandler windowHandler;
    GLFWwindow* window; //pointer to the window, freed on cleanup() in VulkanRenderer just now

    //ISSUES
    //because VulkanEngine references objects in WorldState, we have to instanciate worldstate and the objects before starting the rendering engine
    //this is messy and impractical once ui and different model options are available
    //SOLUTION
    //find all worldstate references in VulkanEngine constructor and init() and remove
    //then move object instanciation to WorldState
    //this means we can then load parts of the rendering engine we need first, show a gui, and then load the simulation along with final texture loading etc for the renderer

    WorldState worldState;
    WorldInput worldInput = WorldInput(WIDTH, HEIGHT, window, &worldState);

    worldState.addObject(worldState.objects, WorldObject{glm::vec3(0,0,0), glm::vec3(10000,10000,10000)}); //skybox, scale is scene size effectively
    worldState.addObject(worldState.objects, WorldObject{glm::vec3(-5000,0,0), glm::vec3(100,100,100)}); //star
   // worldState.objects[0].rot = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1,0,0));
    worldState.addObject(worldState.objects, WorldObject{glm::vec3(75,75,20), glm::vec3(0.5f,0.5f,0.5f)}); //lander aka box
    worldState.addObject(worldState.objects, WorldObject{glm::vec3(3900,100,0), glm::vec3(0.2f,0.2f,0.2f)}); //satellite
    worldState.objects[3].velocity = glm::vec3(1,0,0);
    //worldState.objects[2].rot = glm::vec3(0 , 0, glm::radians(90.0f));
    worldState.addObject(worldState.objects, WorldObject{glm::vec3(0,0,0), glm::vec3(20,20,20)}); //asteroid, set to origin

    try{
        window = windowHandler.initWindow(WIDTH, HEIGHT, "Vulkan");
        VulkanEngine engine = VulkanEngine(window, worldState); //instanciate render engine
        engine.init(); //initialise engine (note that model and texture loading might need moved out of init, when we have multiple options and a menu)

        //ISSUE
        // worldState.loadCollisionMeshes(engine) is needed after loading the models in to the engine, we want to be able to do this in the main loop in respone to loading complete or a button being pressed
        //SOLUTION
        //move to main while loop. trigger in response to an event, probably need to refactor the rendering loop

        worldState.loadCollisionMeshes(engine); //once models are loaded in engine worldState can reuse for creating asteroid collision mesh (will probs need moved too)

        //set glfw callbacks for input and then capture the mouse 
        glfwSetKeyCallback(window, WorldInput::key_callback);
        glfwSetCursorPosCallback(window, WorldInput::mouse_callback);
        glfwSetScrollCallback(window, WorldInput::scroll_callback);
    
        while (!glfwWindowShouldClose(window)){ //&& appRunning)
            glfwPollEvents(); //keep polling window events
            worldState.mainLoop(); //progress world state
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