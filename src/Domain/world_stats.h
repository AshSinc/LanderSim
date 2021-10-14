#pragma once

struct WorldStats{
        float timeStepMultiplier = 1;
        float gravitationalForce = 0;
        float landerVelocity = 0;
        float lastImpactForce = 0;
        float largestImpactForce = 0;
        btVector3 landerUp; //these need moved out of here now, they have been added in obj as glm::vec3
        btVector3 landerForward;
};