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
#include <glm/gtx/vector_angle.hpp> //for vectorangle
#include <chrono>
#include <thread>
#include <iostream>

using namespace Ai;

void LanderAi::init(Mediator* mediator, LanderObj* lander){
    p_mediator = mediator;
    p_lander = lander;
    p_landingSite = p_mediator->scene_getLandingSiteObject();
    scaledApproachDistance = APPROACH_DISTANCE*(p_mediator->scene_getFocusableObject("Asteroid").scale.x*2);
}

void LanderAi::landerSimulationTick(btRigidBody* body, float timeStep){
    //setRotation();
    //glm::mat4 bulletHoleRotationMatrix = glm::lookAt(p_lander->pos, p_landingSite->up+p_landingSite->pos, glm::vec3(0.0f, 1.0f, 0.0f));
    //p_lander->rot = bulletHoleRotationMatrix;

    //setting to landing site rotation isnt right, 
    //need to construct new matrix that faces the landing site,

    if(autopilot){
        p_lander->rot = p_landingSite->rot;
        
        glm::mat4 scale = glm::scale(glm::mat4{ 1.0 }, p_lander->scale);
        glm::mat4 translation = glm::translate(glm::mat4{ 1.0 }, p_lander->pos);
        glm::mat4 rotation = p_lander->rot;
        p_lander->transformMatrix = translation * rotation * scale;
        //body->setWorldTransform(Service::glmToBulletT(p_lander->transformMatrix));
        //glm::mat4 look = glm::lookAt(p_lander->pos, p_landingSite->pos, p_lander->up);
        //p_lander->rot = look;
        p_lander->landerTransform.setBasis(Service::glmToBullet(p_lander->rot));
        body->setCenterOfMassTransform(Service::glmToBulletT(p_lander->transformMatrix));
    }

    if(touchdownBoost){
        //raycast the ground below
        //for now we cheat and just calculate to landing site, wont be as good but might indicate viability
        //
        float distanceToLandingSite = glm::length(p_landingSite->pos - p_lander->pos);
        std::cout << distanceToLandingSite << " dist to landing site\n";
        if (distanceToLandingSite < 2.0f){
            std::cout << "Boosting Reverse \n";
            //thrust oppositite vertical direction
            glm::vec3 reverse = glm::normalize(-p_lander->landerVelocityVector)*10.0f;
            p_lander->addImpulseToLanderQueue(1.0f, reverse.x, reverse.y, reverse.z, false);
            touchdownBoost = false;
            autopilot = false;
        }
            
    }
    




    if(imagingActive)
        imagingTimer(timeStep);
}

//this should be renamed, its more than an image timer
void LanderAi::imagingTimer(float timeStep){
    imagingTime += timeStep;
    if(imagingTime > IMAGING_TIMER_SECONDS){
        std::cout << "Image Requested\n";
        imagingTime = 0;
        //p_mediator->renderer_setShouldDrawOffscreen(true);

        if(autopilot){
            calculateMovement();
            //calculateRotation();
        }
    }
}

void LanderAi::setRotation(){
    //glm::vec3 camPos = p_cameraData->cameraPos;
    //glm::mat4 view = glm::lookAt(camPos, camPos + p_cameraData->cameraFront, p_cameraData->cameraUp);
    p_lander->rot = p_landingSite->rot;
    glm::mat4 scale = glm::scale(glm::mat4{ 1.0 }, p_lander->scale);
    glm::mat4 translation = glm::translate(glm::mat4{ 1.0 }, p_lander->pos);
    glm::mat4 rotation = p_lander->rot;
    
    p_lander->transformMatrix = translation * rotation * scale;

    //glm::mat4 look = glm::lookAt(p_lander->pos, p_landingSite->pos, p_lander->up);
    //p_lander->rot = look;
    //m_Body->setCenterOfMassTransform(trans);
    //p_lander->landerTransform.setBasis(Service::glmToBullet(p_lander->rot));
}

void LanderAi::calculateRotation(){
    std::cout << glm::to_string(p_lander->landerAngularVelocity) << "\n";

    glm::vec3 desiredDirection = glm::normalize(p_lander->pos - p_landingSite->pos);

    std::cout << glm::to_string(desiredDirection) << "\n";

    glm::vec3 difference = desiredDirection - p_lander->landerAngularVelocity;

    glm::mat4 inv_transform = glm::inverse(p_lander->transformMatrix);
    difference = inv_transform * glm::vec4(difference, 0.0f);

    p_lander->addImpulseToLanderQueue(1.0f, difference.x, difference.y, difference.z, true);

    //glm::vec3 movementDirection = desiredPosition - p_lander->pos;


    /*p_lander->rot = p_landingSite->rot;

    glm::mat4 scale = glm::scale(glm::mat4{ 1.0 }, p_lander->scale);
    glm::mat4 translation = glm::translate(glm::mat4{ 1.0 }, p_lander->pos);
    glm::mat4 rotation = p_lander->rot;
    
    p_lander->transformMatrix = translation * rotation * scale;

    p_lander->landerTransform.setBasis(Service::glmToBullet(p_landingSite->rot));*/
}

void LanderAi::calculateMovement(){
    //these can be in init
    float speedCap = LANDER_SPEED_CAP;
    float approachDistance = scaledApproachDistance;
    float minRange = scaledApproachDistance - 5;
    float maxRange = scaledApproachDistance + 5;

    float distanceToLandingSite = glm::length(p_landingSite->pos - p_lander->pos);
    //std::cout << distanceToLandingSite << " distance to site\n";

    float angleFromUpVector = glm::angle(p_landingSite->up, glm::normalize( p_lander->pos- p_landingSite->pos));
    //std::cout << angleFromUpVector << " angleFromUpVector\n";
    if(angleFromUpVector < 0.1f)
        if(distanceToLandingSite > minRange && distanceToLandingSite < maxRange){
            descentTicker+=1;
            if(descentTicker > DESCENT_TICKER){
                shouldDescend = true;
                //speedCap = 0.1f; //<---- no this should be vertical speed cap, need to seperate out the components
            }
        }
   
    if(shouldDescend){
        //float distReduction = FINAL_APPROACH_DISTANCE_REDUCTION
        approachDistance = distanceToLandingSite-FINAL_APPROACH_DISTANCE_REDUCTION;
        
        //std::cout << approachDistance << " fastSqrt distance to site\n";
    }

    glm::vec3 desiredDistance(approachDistance, approachDistance, approachDistance); //set an initial approach distance, once stable here then we can start descent
    //glm::vec3 desiredDistance(distanceToLandingSite, distanceToLandingSite, distanceToLandingSite); //set an initial approach distance, once stable here then we can start descent
    glm::vec3 desiredPosition = p_landingSite->pos+(p_landingSite->up*desiredDistance);
    glm::vec3 movementDirection = desiredPosition - p_lander->pos;
    
    //if(glm::length(movementDirection) > 0.5f){
        //float logLength = log10(glm::length(movementDirection));
        //movementDirection = glm::normalize(movementDirection)*logLength;
    //}

    //actually this is pretty good with apply Force rather than Impulse
    if(glm::length(movementDirection) > speedCap)
        movementDirection = glm::normalize(movementDirection) * speedCap;   

    //calculate the force needed to achieve movementDirection vector by subtracting landerVelocityVector
    glm::vec3 difference = movementDirection - p_lander->landerVelocityVector;

    //std::cout << glm::length(difference) <<  " difference length\n";

    //adjust this vector to be relative to lander orientation
    //to do so we take the inverse of the transformation matrix and apply to the desired movement vector to 
    //rotate it to the relative orientation
    glm::mat4 inv_transform = glm::inverse(p_lander->transformMatrix);
    difference = inv_transform * glm::vec4(difference, 0.0f);
    //std::cout << glm::to_string(inv_transform) << "\n";

    p_lander->addImpulseToLanderQueue(1.0f, difference.x, difference.y, difference.z, false);

    //if(outputActionToFile)
}

void LanderAi::setAutopilot(bool b){
    autopilot = b;
}