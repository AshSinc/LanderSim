#include "ai_lander.h"
#include "mediator.h"
#include "obj_lander.h"
#include "obj_landingSite.h"
#include <glm/gtx/string_cast.hpp>

#define GLM_FORCE_RADIANS //makes sure GLM uses radians to avoid confusion
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES //forces GLM to use a version of vec2 and mat4 that have the correct alignment requirements for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE //forces GLM to use depth range of 0 to 1, instead of -1 to 1 as in OpenGL
#include <glm/glm.hpp>

#include <chrono>
#include <thread>
#include <iostream>
#include <glm/gtx/norm.hpp> //for glm::length2

using namespace Ai;

void LanderAi::init(Mediator* mediator, LanderObj* lander){
    p_mediator = mediator;
    p_lander = lander;
    p_landingSite = p_mediator->scene_getLandingSiteObject(); //once LandingSite is an LandingSite: WorldObject in obj_landingsite.h
    //for now we just use mediator to fetch a reference to the world object

    //do setting up stuff

    //maybe get pointers to lander, landing site, asteroid objects?
    //startImagingTimer();
}

//need to add calls to submit boost requests to the queue, queue should be held in or obj lander

void LanderAi::landerSimulationTick(float timeStep){
    if(imagingActive)
        imagingTimer(timeStep);

    //if(autopilot)
    //    calculateMovement();

    //we may need to do something epr tick
    //std::cout << glm::to_string(p_lander->up);
}

void LanderAi::imagingTimer(float timeStep){
    //std::cout << imagingTime << "Image time\n";
    //std::cout << timeStep << "timestep\n";
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

    
    //basically we want final movement vector to be increasingly smaller based on movementVector, but still be close to 1 at most distances
    //what we should do is log length of movementDirection, then normalise movement direction, then scale by the log length
    //smoothed but might be an issue when rotating
    //float logLength = log10(glm::length(movementDirection));
    //movementDirection = glm::normalize(movementDirection)*logLength;
    
    //if(glm::length(movementDirection) > 0.5f){
        //float logLength = log10(glm::length(movementDirection));
        //movementDirection = glm::normalize(movementDirection)*logLength;
    //}
    //else{
    //
    //}

    //float logLength = log10(glm::length(movementDirection));
    //std::cout << glm::length(movementDirection) << "length\n";
    //std::cout << logLength << "Log length\n";
    //movementDirection = glm::normalize(movementDirection)*logLength;
    //std::cout << glm::length(movementDirection) << "modified length\n";
    

    //not smoothed but may be better for training output as its a truer representation of movement required to stay in position
    //although if we can find a solution for the log equilibrium problem then above method would be nicer
    //std::cout << glm::length(movementDirection) << "before length\n";

    //if(glm::length(movementDirection) > 0.25f)
    //    movementDirection = glm::normalize(movementDirection);    

    //std::cout << glm::length(movementDirection) << "after length\n";

    //before we submit movement we should normalize the desired movement vec and subtract our actual velocity from it
    glm::vec3 difference = movementDirection - p_lander->landerVelocityVector;

    p_lander->addImpulseToLanderQueue(1.0f, difference.x, difference.y, difference.z, false);
}