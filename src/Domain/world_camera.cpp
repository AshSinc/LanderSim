#include "world_camera.h"
#include <algorithm>
#include "world_physics.h"
#include "mediator.h"
#include "obj.h"

WorldCamera::WorldCamera(Mediator& mediator) : r_mediator{mediator}{}

void WorldCamera::init(){
   updateAutoCamera(fixedLookRadius);
}

void WorldCamera::updateAutoCamera(float cameraDistanceScale){
    WorldObject& r_object = r_mediator.scene_getFocusableObject(FOCUS_NAMES[objectFocusIndex]);
    
    glm::vec3 direction;
    glm::vec4 cameraPos;

    if(FOCUS_NAMES[objectFocusIndex] == "Landing_Site"){
        direction = -r_object.up;
        glm::vec3 dirScaled = direction * cameraDistanceScale;
        glm::mat4 trans = glm::mat4(1.0f);
        trans = glm::translate(trans, -dirScaled);
        cameraPos = glm::mat4(trans)*glm::vec4(r_object.pos, 1.0f);
    }
    else{
        WorldObject& r_object2 = r_mediator.scene_getFocusableObject("Asteroid");
        direction = normalize(r_object2.pos-r_object.pos);
        glm::vec3 dirScaled = direction * cameraDistanceScale;
        glm::mat4 trans = glm::mat4(1.0f);
        trans = glm::translate(trans, -dirScaled);
        cameraPos = glm::mat4(trans)*glm::vec4(r_object.pos, 1.0f);
    }
    updateCamera(cameraPos, direction);
}

void WorldCamera::setMouseLookActive(bool state){
    mouselookActive = state;
    firstMouseInput = true;
}   

void WorldCamera::toggleAutoCamera(){
    usingAutoCamera = !usingAutoCamera;
}   

void WorldCamera::updateFixedLookPosition(){
    WorldObject& r_object = r_mediator.scene_getFocusableObject(FOCUS_NAMES[objectFocusIndex]);
    float fixedObjectScaleFactor = std::max(r_object.scale.x/8, 1.0f);
    glm::vec3 newPos;
    if(usingAutoCamera){
        updateAutoCamera(fixedLookRadius*fixedLookRadius*fixedObjectScaleFactor);
    }
    else{
        
        float offsetPitch = pitch - 90; //have to rotate the pitch by 90 degrees down to allow it to travel under the plane
        newPos.x = fixedLookRadius*fixedLookRadius * fixedObjectScaleFactor * cos(glm::radians(yaw)) * sin(glm::radians(offsetPitch)) + r_object.pos.x;
        newPos.y = fixedLookRadius*fixedLookRadius * fixedObjectScaleFactor * sin(glm::radians(yaw)) * sin(glm::radians(offsetPitch)) + r_object.pos.y;
        newPos.z = fixedLookRadius*fixedLookRadius * fixedObjectScaleFactor * cos(glm::radians(offsetPitch)) + r_object.pos.z;
        updateCamera(newPos, normalize(r_object.pos - newPos));
    }
}

//switches focus object when not in freelook
void WorldCamera::changeFocus(){
    if(!freelook){//freelook isnt used but could be at some point
        objectFocusIndex++;
        if(objectFocusIndex == FOCUS_NAMES_SIZE) //if we are at array capacity
            objectFocusIndex = 0;
    }
}

void WorldCamera::updateCamera(glm::vec3 newPos, glm::vec3 front){
    cameraData.cameraPos = newPos;
    cameraData.cameraFront = front;
}

void WorldCamera::calculateZoom(float offset){
    if(!freelook){
        fixedLookRadius -= offset * MOUSESCROLL_SENSITIVITY;
        if(fixedLookRadius > MAX_FIXED_LOOK_RADIUS)
            fixedLookRadius = MAX_FIXED_LOOK_RADIUS;
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
        xOffset = xpos - lastMouseX;
        yOffset = ypos - lastMouseY;
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
        //constrain the yaw so it doesnt escape 0-360
        if(yaw > 360.0f)
            yaw = yaw - 360.0f;
        if(yaw < 0.0f)
            yaw = 360.0f + yaw;
    }
}