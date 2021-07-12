#include "world_input.h"
#include "world_state.h"
#define GLM_FORCE_RADIANS //makes sure GLM uses radians to avoid confusion
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES //forces GLM to use a version of vec2 and mat4 that have the correct alignment requirements for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE //forces GLM to use depth range of 0 to 1, instead of -1 to 1 as in OpenGL
#include <glm/glm.hpp>
#include <iostream>
#include <algorithm>


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
bool WorldInput::freelook = false;
int WorldInput::objectFocusIndex = 2; //start at the first worldState.object[i] that we want to be able to track, have to set in update function too
bool WorldInput::menuVisible = true;

WorldInput::WorldInput(uint32_t& winHeight, uint32_t& winWidth, GLFWwindow* window) : winHeight{winHeight}, winWidth{winWidth}{
    lastMouseX = winHeight/2;
    lastMouseY = winWidth/2;
    window = window;
}

void WorldInput::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods){
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS){
        toggleMenu(window);
    }
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS){
        changeFocus();
    }
    if (key == GLFW_KEY_V && action == GLFW_PRESS){
        freelook = !freelook;
    }
    if(freelook){
        const float cameraSpeed = CAMERA_SPEED * WorldState::deltaTime;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camera.cameraPos += cameraSpeed * camera.cameraFront; //need to track speed and accel, and move in a seperate thread
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camera.cameraPos -= cameraSpeed * camera.cameraFront;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camera.cameraPos -= glm::normalize(glm::cross(camera.cameraFront, camera.cameraUp)) * cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camera.cameraPos += glm::normalize(glm::cross(camera.cameraFront, camera.cameraUp)) * cameraSpeed;
    }
}

void WorldInput::mouse_callback(GLFWwindow* window, double xpos, double ypos){
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
    
    if(freelook){
        //using trig we calculate yaw and roll, soh cah toa rules, need to add roll at some point, had to flip z and y directions, probably opengl to vulkan thing
        glm::vec3 direction;
        direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        direction.y = -sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        direction.z = -sin(glm::radians(pitch));
        camera.cameraFront = glm::normalize(direction);
    }
    else{
        updateFixedLookPosition();
    }
}

void WorldInput::updateFixedLookPosition(){
    glm::vec3 newPos;
    float offsetPitch = pitch - 90; //have to rotate the pitch by 90 degrees down to allow it to travel under the plane
    newPos.x = fixedLookRadius * fixedObjectScaleFactor * cos(glm::radians(yaw)) * sin(glm::radians(offsetPitch)) + world.objects[objectFocusIndex].pos.x;
    newPos.y = fixedLookRadius * fixedObjectScaleFactor * sin(glm::radians(yaw)) * sin(glm::radians(offsetPitch)) + world.objects[objectFocusIndex].pos.y;
    newPos.z = fixedLookRadius * fixedObjectScaleFactor * cos(glm::radians(offsetPitch)) + world.objects[objectFocusIndex].pos.z;
    camera.cameraPos = newPos;
    //then look at the object
    //note should allow switching of the focused object
    camera.cameraFront = normalize(world.objects[objectFocusIndex].pos - newPos); 
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

//toggles menu on and off
void WorldInput::toggleMenu(GLFWwindow* window){
    if(menuVisible){
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); //capture the cursor, for mouse movement
        menuVisible = false;
    }
    else{
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); //capture the cursor, for mouse movement
        menuVisible = true;
    }
}