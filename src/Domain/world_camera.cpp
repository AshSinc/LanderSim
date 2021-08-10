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
    WorldObject& r_object = r_mediator.scene_getWorldObject(objectFocusIndex);
    //WorldObject& r_object = r_mediator.scene_getFocusableObject("Lander");
    WorldObject& r_object2 = r_mediator.scene_getFocusableObject("Asteroid");
    glm::vec3 directionToAsteroid = normalize(r_object2.pos-r_object.pos);
    glm::vec3 dirScaled = directionToAsteroid * cameraDistanceScale;
    glm::mat4 trans = glm::mat4(1.0f);
    trans = glm::translate(trans, -dirScaled);
    glm::vec4 cameraPosV4 = glm::mat4(trans)*glm::vec4(r_object.pos, 1.0f);
    updateCamera(cameraPosV4, directionToAsteroid);

    //glm::quat temp1 = glm::normalize( glm::quat((GLfloat)( -xOffset), glm::vec3(0.0, 1.0, 0.0)) );
    //glm::quat temp2 = glm::normalize( glm::quat((GLfloat)( -yOffset), dir_norm) );
    //glm::vec4 camDirection = temp2 * (temp1 * directionToAsteroid * glm::inverse(temp1)) * glm::inverse(temp2);
    //updateCamera();
    
    /*r_object.printString();
    std::cout << "\n";
    r_object2.printString();
    std::cout << "\n";
    std::cout << "ObjID: The Camera" << "\n";
    std::cout << "pos: (" << cameraData.cameraPos.x << ", " << cameraData.cameraPos.y << ", " << cameraData.cameraPos.z << ")\n";
    std::cout << "\n\n\n";*/
}

void WorldCamera::setMouseLookActive(bool state){
    mouselookActive = state;
    firstMouseInput = true;
}   

void WorldCamera::toggleAutoCamera(){
    usingAutoCamera = !usingAutoCamera;
}   

void WorldCamera::updateFixedLookPosition(){
    WorldObject& r_object = r_mediator.scene_getWorldObject(objectFocusIndex);
    float fixedObjectScaleFactor = std::max(r_object.scale.x, 1.0f);
    glm::vec3 newPos;
    if(usingAutoCamera){
        updateAutoCamera(fixedLookRadius*fixedLookRadius*fixedObjectScaleFactor);
    }
    else{
        
        float offsetPitch = pitch - 90; //have to rotate the pitch by 90 degrees down to allow it to travel under the plane
        newPos.x = fixedLookRadius*fixedLookRadius * fixedObjectScaleFactor * cos(glm::radians(yaw)) * sin(glm::radians(offsetPitch)) + r_object.pos.x;
        newPos.y = fixedLookRadius*fixedLookRadius * fixedObjectScaleFactor * sin(glm::radians(yaw)) * sin(glm::radians(offsetPitch)) + r_object.pos.y;
        newPos.z = fixedLookRadius*fixedLookRadius * fixedObjectScaleFactor * cos(glm::radians(offsetPitch)) + r_object.pos.z;

        //cameraData.cameraUp = normalize(glm::vec3(-r_object.pos));
        updateCamera(newPos, normalize(r_object.pos - newPos));
    }
}

//switches focus object when not in freelook
void WorldCamera::changeFocus(){
    int num_objects = r_mediator.scene_getWorldObjectsCount();
    if(!freelook){
        objectFocusIndex++;
        if(objectFocusIndex == num_objects)
            objectFocusIndex = 2; //this shouldnt be hardcoded, for now Skybox and Star are object 0 and 1, so 2 onwards should be ok
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