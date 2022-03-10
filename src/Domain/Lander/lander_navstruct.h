#pragma once
#include <glm/glm.hpp>

struct NavigationStruct{
    float approachDistance;
    glm::vec3 angularVelocityOfAsteroid;
    glm::vec3 angularVelocityOfAsteroid_Estimate;
    glm::vec3 gravityVector;
    glm::mat4 landerTransformMatrix;
    glm::vec3 velocityVector;
    glm::vec3 landingSitePos;
    glm::vec3 landingSiteUp;
    glm::vec3 landingSitePos_Estimate;
    glm::vec3 landingSiteUp_Estimate;
    glm::vec3 landingSitePos_AtDescentStart;
    glm::vec3 landingSiteUp_AtDescentStart;
    float altitude;
    float radiusAtOpticalCenter;
    bool estimationComplete = false;
    int asteroidScale;
    glm::vec3 landerPos;
    bool useOnlyEstimate = false;
};

struct LanderBoostCommand{
    float duration;
    glm::vec3 vector;
    bool torque; //true if rotation
};