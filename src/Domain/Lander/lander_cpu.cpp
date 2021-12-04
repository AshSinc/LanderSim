#include "lander_cpu.h"
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

using namespace Lander;

void CPU::init(Mediator* mediator, LanderObj* lander){
    p_mediator = mediator;
    p_lander = lander;
    p_landingSite = p_mediator->scene_getLandingSiteObject();
    approachDistance = INITIAL_APPROACH_DISTANCE*(p_mediator->scene_getFocusableObject("Asteroid").scale.x*2);
    asteroidAngularVelocity = Service::bt2glm(p_landingSite->angularVelocity);
    gnc.init(mediator, &navStruct);
    cv.init(mediator);
}

void CPU::simulationTick(btRigidBody* body, float timeStep){
    //setting to landing site rotation isnt right, 
    //need to construct new matrix that faces the landing site,

    if(autopilot){
        //setRotation(); //we are cheating and locking rotation to landing s
        //body->setCenterOfMassTransform(Service::glmToBulletT(p_lander->transformMatrix));
    }
    
    if(imagingTimer(timeStep)){
        p_mediator->renderer_setShouldDrawOffscreen(true);
        //camera.tick(timestep);
    }
    if(gncTimer(timeStep)){
        navStruct.landingSitePos = p_mediator->physics_performRayCast(p_landingSite->pos, -p_landingSite->up, 10.0f); //raycast from landing site past ground (ie -up*10)
        navStruct.landingSiteUp = p_landingSite->up;
        navStruct.angularVelocity = asteroidAngularVelocity;
        navStruct.approachDistance = approachDistance;
        navStruct.gravityVector = Service::bt2glm(p_lander->landerGravityVector);
        navStruct.landerTransformMatrix = p_lander->transformMatrix;
        navStruct.velocityVector = p_lander->landerVelocityVector;
        //std::cout << glm::to_string(p_lander->pos) << "  cpu pos \n";

        glm::vec3 calculatedThrustVector = gnc.getThrustVector(GNC_TIMER_SECONDS);
        if(glm::length(calculatedThrustVector) != 0);
            addImpulseToLanderQueue(1.0f, calculatedThrustVector.x, calculatedThrustVector.y, calculatedThrustVector.z, false);
    }

    //check if we have a boost command queued
    //this and corresponding queue need moved to Lander::CPU
    if(landerImpulseRequested()){
        LanderBoostCommand& nextBoost = popLanderImpulseQueue();
        if(nextBoost.torque)
            applyTorque(body, nextBoost);
        else
            applyImpulse(body, nextBoost);
    }
}

void CPU::addImpulseToLanderQueue(float duration, float x, float y, float z, bool torque){
    landerBoostQueueLock.lock();
    landerBoostQueue.push_back(LanderBoostCommand{duration, glm::vec3{x,y,z}, torque});
    landerBoostQueueLock.unlock();
}

bool CPU::landerImpulseRequested(){
    landerBoostQueueLock.lock();
    bool empty = landerBoostQueue.empty();
    landerBoostQueueLock.unlock();
    return !empty;
}

LanderBoostCommand& CPU::popLanderImpulseQueue(){
    landerBoostQueueLock.lock();
    LanderBoostCommand& command = landerBoostQueue.front();
    landerBoostQueue.pop_front();
    landerBoostQueueLock.unlock();
    return command;
}

//this should be renamed, its more than an image timer
bool CPU::gncTimer(float timeStep){
    if(gncActive){
        gncTime += timeStep;
        if(gncTime > GNC_TIMER_SECONDS){
            std::cout << "GNC Requested\n";
            gncTime = 0;
            return true;
        }
    }
    return false;
}

//this should be renamed, its more than an image timer
bool CPU::imagingTimer(float timeStep){
    if(imagingActive){
        imagingTime += timeStep;
        if(imagingTime > IMAGING_TIMER_SECONDS){
            std::cout << "Image Requested\n";
            imagingTime = 0;
            return true;
        }
    }
    return false;
}

void CPU::setRotation(){
    p_lander->rot = p_landingSite->rot;//we are cheating by setting the lander to rotation matrix, to the landing sites rot matrix
    //then we construct the transform matrix from this, and set for both renderer and bullet
    glm::mat4 scale = glm::scale(glm::mat4{ 1.0 }, p_lander->scale);
    glm::mat4 translation = glm::translate(glm::mat4{ 1.0 }, p_lander->pos);
    glm::mat4 rotation = p_lander->rot;
    p_lander->transformMatrix = translation * rotation * scale; //renderer transform
    p_lander->landerTransform.setBasis(Service::glmToBullet(p_lander->rot)); //bullet transform

    //ISSUE
    //While this means the contact is always perfect at touchdown, it means there is always unnatural rotation before that point
    //cant just set this right before landing either because the model would snap to a different rotation, would look even worse
    //
    //SOLUTION
    //check 2 or 3 times on approach, and align to expected tgo up vector
    //by calculating offset between current up vector and desired up vector and submitting a correction. 
    //if we are using Reaction Wheels, we dont need to submit torque commands, just slerp the rotation every tick
    //lerp or slerp towards desired rotation at touchdown time, maybe a minute before touchdown
    //still not sure of the maths though or even how to approach this one
}

//these functions could be combined
void CPU::applyImpulse(btRigidBody* rigidbody, LanderBoostCommand boost){       
    btVector3 boostVector = Service::glm2bt(boost.vector);
    //std::cout << boostVector.length() << " boost vec\n";
    float force = BOOST_STRENGTH * boostVector.length();
    force = glm::clamp(force, 0.0f, LANDER_BOOST_CAP); //clamp boost force to a max value

    if(force > 0.0f){
        btVector3 boostDirection = boostVector.normalize();
        btMatrix3x3& rot = rigidbody->getWorldTransform().getBasis();
        btVector3 correctedForce = rot * boostDirection;
        //rigidbody->applyCentralForce(correctedForce*force);
        rigidbody->applyCentralImpulse(correctedForce*force);
    }
    else{
        std::cout << "This is strange, applyImpulse is being triggered when pausing/unpause or changing time\n";
    }
}

//dont need to use this, we will simulate reaction wheel behaviour instead
void CPU::applyTorque(btRigidBody* rigidbody, LanderBoostCommand boost){
    btVector3 relativeForce = Service::glm2bt(boost.vector);
    btMatrix3x3& rot = rigidbody->getWorldTransform().getBasis();
    btVector3 correctedForce = rot * relativeForce;
    rigidbody->applyTorqueImpulse(correctedForce);
}

void CPU::setAutopilot(bool b){
    autopilot = b;
    gncActive = b;//maybe need to move later
}

void CPU::setImaging(bool b){
    imagingActive = b;
}