#include "ui_input.h"
#include <GLFW/glfw3.h>
#include "mediator.h"
#include <iostream>

//regular functions, callbacks from main.cpp for glfw, which needs either regular functions or static because its written in C
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods){
    UiInput* input = (UiInput*)glfwGetWindowUserPointer(window);
    if(action == GLFW_PRESS || action == GLFW_REPEAT){// && action != GLFW_RELEASE)// || action == GLFW_REPEAT)
        input->processKey(key, action == GLFW_REPEAT);
    }
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

void UiInput::processKeyGroupMovement(int key, bool repeatKeypress){
    float x = 0, y = 0, z = 0;
    float duration = 1;
    bool torque = false;
   if(!(repeatKeypress && !landingSiteInput)){ //need to stop repeat keys on lander controls
        if (key == GLFW_KEY_KP_8)
            z=1.0f;
        if (key == GLFW_KEY_KP_2)
            z=-1.0f;
        if (key == GLFW_KEY_KP_4)
            x=-1.0f;
        if (key == GLFW_KEY_KP_6)
            x=1.0f;
        if (key == GLFW_KEY_KP_7)
            y=-1.0f;
        if (key == GLFW_KEY_KP_9)
            y=1.0f;
        if (key == GLFW_KEY_LEFT){
            z=-1.0f;
            torque = true;
        }
        if (key == GLFW_KEY_RIGHT){
            z=1.0f;
            torque = true;
        }
        if (key == GLFW_KEY_UP){
            y=1.0f;
            torque = true;
        }
        if (key == GLFW_KEY_DOWN){
            y=-1.0f;
            torque = true;
        }
    }

    //if there is a valid input direction
    if(x != 0 && y != 0 && z != 0)
        if(landingSiteInput)
                r_mediator.physics_moveLandingSite(x, y, z, torque);
            else
                r_mediator.physics_addImpulseToLanderQueue(duration, x, y, z, torque);
}
void UiInput::processKey(int key, bool repeatKeypress){
    processKeyGroupMovement(key, repeatKeypress);
    if(!repeatKeypress){
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
        if (key == GLFW_KEY_O)
            r_mediator.camera_toggleAutoCamera();
        //if (key == GLFW_KEY_I)
            //r_mediator.renderer_writeOffscreenImageToDisk();
            //r_mediator.renderer_setShouldDrawOffscreen(true);
        if (key == GLFW_KEY_M)
            landingSiteInput = !landingSiteInput;
    }    
}

void UiInput::scrollMoved(double xoffset, double yoffset){
    r_mediator.camera_calculateZoom(yoffset);
}

void UiInput::mouseMoved(double xpos, double ypos){
    r_mediator.camera_calculatePitchYaw(xpos, ypos);
}