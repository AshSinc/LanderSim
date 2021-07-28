#include "world_input.h"
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS //makes sure GLM uses radians to avoid confusion
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES //forces GLM to use a version of vec2 and mat4 that have the correct alignment requirements for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE //forces GLM to use depth range of 0 to 1, instead of -1 to 1 as in OpenGL
#include <glm/glm.hpp>
#include <iostream>
#include <algorithm>
#include "world_state.h"
#include "ui_handler.h"
#include "world_camera.h"

//regular functions, callbacks from main.cpp for glfw, which needs either regular functions or static because its written in C
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods){
    WorldInput* input = (WorldInput*)glfwGetWindowUserPointer(window);
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        input->toggleMenu();
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
        input->changeFocus();
    if (key == GLFW_KEY_LEFT_BRACKET && action == GLFW_PRESS)
        input->changeSimSpeed(-1,false);
    if (key == GLFW_KEY_RIGHT_BRACKET && action == GLFW_PRESS)
        input->changeSimSpeed(1,false);
    if (key == GLFW_KEY_P && action == GLFW_PRESS)
        input->changeSimSpeed(0, true);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos){
    WorldInput* input = (WorldInput*)glfwGetWindowUserPointer(window);
    input->mouseMoved(xpos, ypos);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset){
    WorldInput* input = (WorldInput*)glfwGetWindowUserPointer(window);
    input->scrollMoved(xoffset, yoffset);
}

WorldInput::WorldInput(GLFWwindow* window){
    //p_window = window;
    glfwGetFramebufferSize(window, &winWidth, &winHeight);
};

void WorldInput::setWorld(WorldState* world){
    p_world = world;
}

void WorldInput::setCamera(WorldCamera* camera){
    p_camera = camera;
}

void WorldInput::setUiHandler(UiHandler* uiHandler){
    p_uiHandler = uiHandler;
}

void WorldInput::scrollMoved(double xoffset, double yoffset){
    p_camera->changeZoom(yoffset * MOUSESCROLL_SENSITIVITY);
}

void WorldInput::mouseMoved(double xpos, double ypos){
    if(!p_uiHandler->getShowEscMenu()){
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
    }
}

void WorldInput::changeSimSpeed(int pos, bool pause){
    p_world->changeSimSpeed(pos, pause);
}

void WorldInput::updateFixedLookPosition(){
    p_camera->updateFixedLookPosition(pitch, yaw);
}

//switches focus object when not in freelook
void WorldInput::changeFocus(){
    p_camera->changeFocus();
}

//toggles menu on and off, should be moved to a UI handler
void WorldInput::toggleMenu(){
    p_uiHandler->toggleMenu();
}