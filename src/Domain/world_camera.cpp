#include "world_camera.h"
#include <algorithm>
#include "world_physics.h"
#include "mediator.h"
#include "obj.h"

WorldCamera::WorldCamera(Mediator& mediator) : r_mediator{mediator}{}

void WorldCamera::setMouseLookActive(bool state){
    mouselookActive = state;
    firstMouseInput = true;
}   

void WorldCamera::updateFixedLookPosition(){ //pass the focusedObject.pos vec3 and scale?
    WorldObject& r_object = r_mediator.physics_getWorldObject(objectFocusIndex);
    float fixedObjectScaleFactor = std::max(r_object.scale.x, 1.0f);
    glm::vec3 newPos;
    float offsetPitch = pitch - 90; //have to rotate the pitch by 90 degrees down to allow it to travel under the plane
    newPos.x = fixedLookRadius * fixedObjectScaleFactor * cos(glm::radians(yaw)) * sin(glm::radians(offsetPitch)) + r_object.pos.x;
    newPos.y = fixedLookRadius * fixedObjectScaleFactor * sin(glm::radians(yaw)) * sin(glm::radians(offsetPitch)) + r_object.pos.y;
    newPos.z = fixedLookRadius * fixedObjectScaleFactor * cos(glm::radians(offsetPitch)) + r_object.pos.z;
    updateCamera(newPos, normalize(r_object.pos - newPos));
}

//switches focus object when not in freelook
void WorldCamera::changeFocus(){
    int num_objects = r_mediator.physics_getWorldObjectsCount();
    if(!freelook){
        objectFocusIndex++;
        if(objectFocusIndex == num_objects)
            objectFocusIndex = 2;
    }
}

void WorldCamera::updateCamera(glm::vec3 newPos, glm::vec3 front){
    cameraData.cameraPos = newPos;
    cameraData.cameraFront = front;
}

void WorldCamera::calculateZoom(float offset){
    if(!freelook){
        fixedLookRadius -= offset * MOUSESCROLL_SENSITIVITY;
        if(fixedLookRadius > 10)
            fixedLookRadius = 10;
        if(fixedLookRadius < 1)
            fixedLookRadius = 1;
    }
}

CameraData* WorldCamera::getCameraDataPtr(){
    return &cameraData;
}

void WorldCamera::calculatePitchYaw(double xpos, double ypos){
    if(mouselookActive){
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