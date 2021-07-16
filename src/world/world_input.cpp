#include "world_input.h"
#include "world_state.h"
#define GLM_FORCE_RADIANS //makes sure GLM uses radians to avoid confusion
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES //forces GLM to use a version of vec2 and mat4 that have the correct alignment requirements for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE //forces GLM to use depth range of 0 to 1, instead of -1 to 1 as in OpenGL
#include <glm/glm.hpp>
#include <iostream>
#include <algorithm>
#include <ui_handler.h>

//class UIHandler;

float WorldInput::SIM_SPEEDS [7] {0.25f,0.5f,1,2,4,8,16};
int WorldInput::selectedSimSpeedIndex = 2; //points to SIM_SPEEDS array
const float WorldInput::CAMERA_SPEED = 5;
const float WorldInput::MOUSELOOK_SENSITIVITY = 0.1f;
const float WorldInput::MOUSESCROLL_SENSITIVITY = 0.5f;
float WorldInput::fixedLookRadius = 2;
float WorldInput::fixedObjectScaleFactor = 1;

WorldCamera& WorldInput::camera = WorldState::getWorldCamera();
std::vector<WorldObject>& WorldInput::objects = WorldState::getWorldObjects();

float WorldInput::lastMouseX;
float WorldInput::lastMouseY;
float WorldInput::yaw = 0.0f;
float WorldInput::pitch = 0.0f;
bool WorldInput::firstMouseInput = true;
bool WorldInput::freelook = false; //no longer in use
int WorldInput::objectFocusIndex = 2; //start at the first worldState.object[i] that we want to be able to track, have to set in update function too

WorldInput::WorldInput(uint32_t& winHeight, uint32_t& winWidth, GLFWwindow* window, WorldState* world) : winHeight{winHeight}, winWidth{winWidth}{
    lastMouseX = winHeight/2;
    lastMouseY = winWidth/2;
    window = window;
    world = world;
}

void WorldInput::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods){
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS){
        toggleMenu(window);
    }
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS){
        changeFocus();
    }
    if (key == GLFW_KEY_LEFT_BRACKET && action == GLFW_PRESS){
        changeSimSpeed(-1,false);
    }
    if (key == GLFW_KEY_RIGHT_BRACKET && action == GLFW_PRESS){
        changeSimSpeed(1,false);
    }
    if (key == GLFW_KEY_P && action == GLFW_PRESS){
        changeSimSpeed(0, true);
    }
}

void WorldInput::changeSimSpeed(int pos, bool pause){
    if(pause){ //toggle pause on and off
        if(world->worldStats.timeStepMultiplier == 0)
            world->worldStats.timeStepMultiplier = SIM_SPEEDS[selectedSimSpeedIndex];
        else
            world->worldStats.timeStepMultiplier = 0;
    }
    else{
        if(pos == 0) //0 will be a normal speed shortcut eventually?
            selectedSimSpeedIndex = 2;
        else{
            selectedSimSpeedIndex+=pos;
            if(selectedSimSpeedIndex < 0)
                selectedSimSpeedIndex = 0;
            else if(selectedSimSpeedIndex > 6) //need to add a var for array init so we dont have a hardcoded size here
                selectedSimSpeedIndex = 6;
            world->worldStats.timeStepMultiplier = SIM_SPEEDS[selectedSimSpeedIndex];
        }
    }
}

void WorldInput::mouse_callback(GLFWwindow* window, double xpos, double ypos){
    if(!UiHandler::showEscMenu){
        if(firstMouseInput){ //checks first mouse input to avoid a jump
            lastMouseX = xpos;
            lastMouseY = ypos;
            firstMouseInput = false;
        }
        //get difference in x and y mouse position from last capture
        float xOffset = xpos - lastMouseX;
        float yOffset = ypos - lastMouseY;
        lastMouseX = xpos;
        lastMouseY = ypos;
        xOffset *= MOUSELOOK_SENSITIVITY;
        yOffset *= MOUSELOOK_SENSITIVITY;
        //adjust yaw and pitch
        yaw += xOffset;
        pitch += yOffset;
        //constrain the pitch so it doesnt flip at 90 degrees
        if(pitch > 89.0f)
            pitch = 89.0f;
        if(pitch < -89.0f)
            pitch = -89.0f;
        //constrain the yaw so it doesnt flip at 90 degrees
        if(yaw > 360.0f)
            yaw = yaw - 360.0f;
        if(yaw < 0.0f)
            yaw = 360.0f + yaw;
        updateFixedLookPosition();
    }
}

void WorldInput::updateFixedLookPosition(){
    glm::vec3 newPos;
    float offsetPitch = pitch - 90; //have to rotate the pitch by 90 degrees down to allow it to travel under the plane
    newPos.x = fixedLookRadius * fixedObjectScaleFactor * cos(glm::radians(yaw)) * sin(glm::radians(offsetPitch)) + world->objects[objectFocusIndex].pos.x;
    newPos.y = fixedLookRadius * fixedObjectScaleFactor * sin(glm::radians(yaw)) * sin(glm::radians(offsetPitch)) + world->objects[objectFocusIndex].pos.y;
    newPos.z = fixedLookRadius * fixedObjectScaleFactor * cos(glm::radians(offsetPitch)) + world->objects[objectFocusIndex].pos.z;
    camera.cameraPos = newPos;
    //then look at the object
    //note should allow switching of the focused object
    camera.cameraFront = normalize(world->objects[objectFocusIndex].pos - newPos); 
}

void WorldInput::scroll_callback(GLFWwindow* window, double xoffset, double yoffset){
    if(!freelook){
        fixedLookRadius -= yoffset * MOUSESCROLL_SENSITIVITY;
        if(fixedLookRadius > 10)
            fixedLookRadius = 10;
        if(fixedLookRadius < 1)
            fixedLookRadius = 1;
        updateFixedLookPosition();
    }
}

//switches focus object when not in freelook
void WorldInput::changeFocus(){
    if(!freelook){
        objectFocusIndex++;
        if(objectFocusIndex == objects.size())
            objectFocusIndex = 2;
        fixedLookRadius = 3; //need to set a default starting value
        fixedObjectScaleFactor = std::max(objects[objectFocusIndex].scale.x, 1.0f);
        updateFixedLookPosition();
    }
}

//toggles menu on and off, should be moved to a UI handler
void WorldInput::toggleMenu(GLFWwindow* window){
    if(UiHandler::showEscMenu){
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); //capture the cursor, for mouse movement
        UiHandler::showEscMenu = false;
        changeSimSpeed(0, true);
    }
    else{
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); //capture the cursor, for mouse movement
        UiHandler::showEscMenu = true;
        changeSimSpeed(0, true);
    }
}