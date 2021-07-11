#include "vk_engine.h"
#include <stdexcept>
#include <iostream>
#include "world_state.h"
#include "world_input.h"

uint32_t WIDTH = 1920; //defaults, doesnt track resizing, should have a callback function here to update these from engine
uint32_t HEIGHT = 1080;

int main(){
    WindowHandler windowHandler;
    GLFWwindow* window; //pointer to the window, freed on cleanup() in VulkanRenderer just now

    WorldState worldState;
    WorldInput worldInput = WorldInput(WIDTH, HEIGHT);

    worldState.addObject(worldState.objects, WorldObject{glm::vec3(0,0,0), glm::vec3(10000,10000,10000)}); //skybox, scale is scene size effectively
    worldState.addObject(worldState.objects, WorldObject{glm::vec3(-5000,0,0), glm::vec3(100,100,100)}); //star
   // worldState.objects[0].rot = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1,0,0));
    worldState.addObject(worldState.objects, WorldObject{glm::vec3(50,0,20), glm::vec3(0.5f,0.5f,0.5f)}); //lander aka box
    worldState.addObject(worldState.objects, WorldObject{glm::vec3(3900,100,0), glm::vec3(0.2f,0.2f,0.2f)}); //satellite
    worldState.objects[3].velocity = glm::vec3(1,0,0);
    //worldState.objects[2].rot = glm::vec3(0 , 0, glm::radians(90.0f));
    worldState.addObject(worldState.objects, WorldObject{glm::vec3(0,0,0), glm::vec3(20,20,20)}); //asteroid, set to origin

    try{
        window = windowHandler.initWindow(WIDTH, HEIGHT, "Vulkan");
        VulkanEngine engine = VulkanEngine(window, worldState); //instanciate render engine
        engine.init(); //initialise engine (note that model and texture loading might need moved out of init, when we have multiple options and a menu)
        worldState.loadCollisionMeshes(engine); //once models are loaded in engine worldState can reuse for creating asteroid collision mesh (will probs need moved too)

        //set glfw callbacks for input and then capture the mouse 
        glfwSetKeyCallback(window, WorldInput::key_callback);
        glfwSetCursorPosCallback(window, WorldInput::mouse_callback);
        glfwSetScrollCallback(window, WorldInput::scroll_callback);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); //capture the cursor, for mouse movement
       
        while (!glfwWindowShouldClose(window)){
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