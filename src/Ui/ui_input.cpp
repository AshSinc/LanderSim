#include "ui_input.h"
#include <GLFW/glfw3.h>
#include "mediator.h"

//regular functions, callbacks from main.cpp for glfw, which needs either regular functions or static because its written in C
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods){
    UiInput* input = (UiInput*)glfwGetWindowUserPointer(window);
    if(action == GLFW_PRESS)
        input->processKey(key);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos){
    UiInput* input = (UiInput*)glfwGetWindowUserPointer(window);
    input->mouseMoved(xpos, ypos);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset){
    UiInput* input = (UiInput*)glfwGetWindowUserPointer(window);
    input->scrollMoved(xoffset, yoffset);
}

UiInput::UiInput(Mediator& mediator): r_mediator{mediator}{};

void UiInput::processKey(int key){
    if (key == GLFW_KEY_ESCAPE)
        r_mediator.ui_toggleEscMenu();
    if (key == GLFW_KEY_SPACE)
        r_mediator.camera_changeFocus();
    if (key == GLFW_KEY_LEFT_BRACKET)
        r_mediator.physics_changeSimSpeed(-1,false);
    if (key == GLFW_KEY_RIGHT_BRACKET)
        r_mediator.physics_changeSimSpeed(1,false);
    if (key == GLFW_KEY_P)
        r_mediator.physics_changeSimSpeed(0, true);
}

void UiInput::scrollMoved(double xoffset, double yoffset){
    r_mediator.camera_calculateZoom(yoffset);
}

void UiInput::mouseMoved(double xpos, double ypos){
    r_mediator.camera_calculatePitchYaw(xpos, ypos);
}