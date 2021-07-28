#include "world_camera.h"
#include <algorithm>
#include "world_state.h"

WorldCamera::WorldCamera(std::vector<WorldObject>& objects) : r_objects{objects}{}

void WorldCamera::updateFixedLookPosition(float pitch, float yaw){ //pass the focusedObject.pos vec3 and scale?
    float fixedObjectScaleFactor = std::max(r_objects.at(objectFocusIndex).scale.x, 1.0f);
    glm::vec3 newPos;
    float offsetPitch = pitch - 90; //have to rotate the pitch by 90 degrees down to allow it to travel under the plane
    newPos.x = fixedLookRadius * fixedObjectScaleFactor * cos(glm::radians(yaw)) * sin(glm::radians(offsetPitch)) + r_objects.at(objectFocusIndex).pos.x;
    newPos.y = fixedLookRadius * fixedObjectScaleFactor * sin(glm::radians(yaw)) * sin(glm::radians(offsetPitch)) + r_objects.at(objectFocusIndex).pos.y;
    newPos.z = fixedLookRadius * fixedObjectScaleFactor * cos(glm::radians(offsetPitch)) + r_objects.at(objectFocusIndex).pos.z;
    updateCamera(newPos, normalize(r_objects.at(objectFocusIndex).pos - newPos));
}

//switches focus object when not in freelook
void WorldCamera::changeFocus(){
    if(!freelook){
        objectFocusIndex++;
        if(objectFocusIndex == r_objects.size())
            objectFocusIndex = 2;
    }
}

void WorldCamera::updateCamera(glm::vec3 newPos, glm::vec3 front){
    cameraData.cameraPos = newPos;
    cameraData.cameraFront = front;
}

void WorldCamera::changeZoom(float offset){
    if(!freelook){
        fixedLookRadius -= offset;
        if(fixedLookRadius > 10)
            fixedLookRadius = 10;
        if(fixedLookRadius < 1)
            fixedLookRadius = 1;
    }
}

CameraData* WorldCamera::getCameraDataPtr(){
    return &cameraData;
}