#pragma once
#include <glm/glm.hpp>

struct NavigationStruct{
    float approachDistance;
    glm::vec3 angularVelocity;
    glm::vec3 gravityVector;
    glm::mat4 landerTransformMatrix;
    glm::vec3 velocityVector;
    glm::vec3 landingSitePos;
    glm::vec3 landingSiteUp;
    float altitude;
    float radiusOfOpticalLock;
    bool estimationComplete = false;
};