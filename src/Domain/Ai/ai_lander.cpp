#include "ai_lander.h"
#include "mediator.h"
#include "obj_lander.h"
#include "obj_landingSite.h"
#define GLM_FORCE_RADIANS //makes sure GLM uses radians to avoid confusion
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES //forces GLM to use a version of vec2 and mat4 that have the correct alignment requirements for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE //forces GLM to use depth range of 0 to 1, instead of -1 to 1 as in OpenGL
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/norm.hpp> //for glm::length2
#include <chrono>
#include <thread>
#include <iostream>

using namespace Ai;

void LanderAi::init(Mediator* mediator, LanderObj* lander){
    p_mediator = mediator;
    p_lander = lander;
    p_landingSite = p_mediator->scene_getLandingSiteObject();
}

void LanderAi::landerSimulationTick(float timeStep){
    if(imagingActive)
        imagingTimer(timeStep);
}

void LanderAi::imagingTimer(float timeStep){
    imagingTime += timeStep;
    if(imagingTime > IMAGING_TIMER_SECONDS){
        std::cout << "Image Requested\n";
        imagingTime = 0;
        p_mediator->renderer_setShouldDrawOffscreen(true);

        if(autopilot)
            calculateMovement();
    }
}

void LanderAi::calculateMovement(){
    glm::vec3 desiredDistance(50.0f, 50.0f, 50.0f); //set an initial approach distance, once stable here then we can start descent
    glm::vec3 desiredPosition = p_landingSite->pos+(p_landingSite->up*desiredDistance);
    glm::vec3 movementDirection = desiredPosition - p_lander->pos;
    
    //if(glm::length(movementDirection) > 0.5f){
        //float logLength = log10(glm::length(movementDirection));
        //movementDirection = glm::normalize(movementDirection)*logLength;
    //}

    //actually this is pretty good with apply Force rather than Impulse
    if(glm::length(movementDirection) > LANDER_SPEED_CAP){
        movementDirection = glm::normalize(movementDirection) * LANDER_SPEED_CAP;   
    } 

    //calculate the force needed to achieve movementDirection vector by subtracting landerVelocityVector
    glm::vec3 difference = movementDirection - p_lander->landerVelocityVector;

    std::cout << glm::length(difference) <<  " difference length\n";

    //may have to adjust this vector to be relative to lander orientation
    //and if so the boost function will also have to take into account the lander orientation to apply boost

    p_lander->addImpulseToLanderQueue(1.0f, difference.x, difference.y, difference.z, false);

    //if(outputActionToFile)
}