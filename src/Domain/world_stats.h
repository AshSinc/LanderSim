#pragma once

struct WorldStats{
        float timeStepMultiplier = 1;
        float gravitationalForce = 0;
        float landerVelocity = 0;
        float lastImpactForce = 0;
        float largestImpactForce = 0;
        glm::vec3 estimatedAngularVelocity = glm::vec3(0); //here to save time, for output on ui
};