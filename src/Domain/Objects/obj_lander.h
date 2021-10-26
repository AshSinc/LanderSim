#pragma once
#include "obj_collisionRender.h"
#include <glm/gtx/fast_square_root.hpp>
#include "sv_randoms.h"
#include "ai_lander.h"
#include <deque>
#include <mutex>

//holds lander impulse request --should be in obj_lander but i'd need to modify flow a lot
struct LanderBoostCommand{
    float duration;
    glm::vec3 vector;
    bool torque; //true if rotation
};

struct LanderObj : virtual CollisionRenderObj{ //this should impliment an interface for 
    Ai::LanderAi ai = Ai::LanderAi();
    bool collisionCourse = false;
    bool randomStartPositions = false;
    double asteroidGravForceMultiplier = 0.5f;
    float startDistance = 150.0f;
    float passDistance = 70.0f;
    float initialSpeed = 1.5f;

    float landerVelocity = 0.0f;
    glm::vec3 landerVelocityVector = glm::vec3(0.0f);
    glm::vec3 landerAngularVelocity = glm::vec3(0.0f);
    float gravitationalForce = 0.0f;

    float landerBoostStrength = 1.0f;
    float landerRotationalBoostStrength = 1.0f;

    std::mutex landerBoostQueueLock;
    std::deque<LanderBoostCommand> landerBoostQueue;

    btTransform landerTransform;
    Mediator* p_mediator;

    void init(btAlignedObjectArray<btCollisionShape*>* collisionShapes, btDiscreteDynamicsWorld* dynamicsWorld, Mediator& r_mediator){
        //create a dynamic rigidbody
        btCollisionShape* collisionShape = new btBoxShape(btVector3(1,1,1));
        //colShape->setMargin(0.05);
        collisionShape->setLocalScaling(btVector3(scale.x, scale.y, scale.z)); //may not be 1:1 scale between bullet and vulkan
        collisionShapes->push_back(collisionShape);

        //create lander transform
        //btTransform transform;
        landerTransform.setIdentity();

        //initTransform(&transform);
        landerTransform.setOrigin(btVector3(pos.x, pos.y, pos.z));
        btQuaternion quat;
        quat.setEulerZYX(0,0,0);
        landerTransform.setRotation(quat);

        btScalar btMass(mass);

        //rigidbody is dynamic if and only if mass is non zero, otherwise static
        bool isDynamic = (btMass != 0.f);
        btVector3 localInertia(0, 0, 0);
        if (isDynamic)
            collisionShape->calculateLocalInertia(btMass, localInertia);

        //using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
        btDefaultMotionState* myMotionState = new btDefaultMotionState(landerTransform);
        btRigidBody::btRigidBodyConstructionInfo rbInfo(btMass, myMotionState, collisionShape, localInertia);
        rbInfo.m_friction = 1.0f;
        rbInfo.m_spinningFriction  = 0.1f;
        //rbInfo.m_rollingFriction = 0.5f;
        btRigidBody* rigidbody = new btRigidBody(rbInfo);
        rigidbody->setActivationState(DISABLE_DEACTIVATION); //stop body from disabling collision, bullet threshholds are pretty loose

        initRigidBody(&landerTransform, rigidbody);

        dynamicsWorld->addRigidBody(rigidbody);

        p_mediator = &r_mediator;

        //initialise ai module
        ai.init(p_mediator, this);
    }

    void updateLanderUpForward(btRigidBody* body){
        //store the lander up vector (for taking images and aligning)
        btTransform landerWorldTransform = body->getWorldTransform();
        int indexUpAxis = 2;
        up = Service::bt2glm(landerWorldTransform.getBasis().getColumn(indexUpAxis));

        //store the lander forward vector (for taking images and aligning)
        int indexFwdAxis = 0; //not sure if this is actually the forward axis, need to test before using it
        forward = Service::bt2glm(landerWorldTransform.getBasis().getColumn(indexFwdAxis));
    }

    void timestepBehaviour(btRigidBody* body, float timeStep){
        //set the gravity for the lander towards the asteroid
        btVector3 direction = -btVector3(pos.x , pos.y, pos.z); //asteroid is always at origin so dir of gravity is aways towards 0
        gravitationalForce = asteroidGravForceMultiplier*glm::fastInverseSqrt(direction.distance(btVector3(0,0,0)));;
        body->setGravity(direction.normalize()*gravitationalForce);
        
        //store the linear velocity, 
        landerVelocity = body->getLinearVelocity().length(); //will need to properly work out the world size 

        landerVelocityVector = Service::bt2glm(body->getLinearVelocity());

        landerAngularVelocity = Service::bt2glm(body->getAngularVelocity());

        //will probably need to store actual velocity vector as well 

        updateLanderUpForward(body);

        ai.landerSimulationTick(body, timeStep); //let the ai have a tick

        //check if we have a boost command queued
        if(landerImpulseRequested()){
            LanderBoostCommand& nextBoost = popLanderImpulseQueue();
            if(nextBoost.torque)
                applyTorque(body, nextBoost);
            else
                applyImpulse(body, nextBoost);
        }
    }

    void updateWorldStats(WorldStats* worldStats){
        //stored for retrival in UI with getGravitationalForce()
        worldStats->landerVelocity = landerVelocity;
        worldStats->gravitationalForce = gravitationalForce;
    }

    //these functions could be combined
    void applyImpulse(btRigidBody* rigidbody, LanderBoostCommand boost){
        btVector3 relativeForce = btVector3(boost.vector.x, boost.vector.y, boost.vector.z);
        relativeForce *= landerBoostStrength*boost.duration; //temporarily using duration as a multiplier
        btMatrix3x3& rot = rigidbody->getWorldTransform().getBasis();
        btVector3 correctedForce = rot * relativeForce;
        rigidbody->applyCentralForce(correctedForce);
    }
    void applyTorque(btRigidBody* rigidbody, LanderBoostCommand boost){
        btVector3 relativeForce = btVector3(boost.vector.x, boost.vector.y, boost.vector.z);
        relativeForce *= landerRotationalBoostStrength;
        btMatrix3x3& rot = rigidbody->getWorldTransform().getBasis();
        btVector3 correctedForce = rot * relativeForce;
        rigidbody->applyTorqueImpulse(correctedForce);
    }

    void initRigidBody(btTransform* transform, btRigidBody* rigidbody){
        btVector3 direction;
        if(collisionCourse)
            direction = -transform->getOrigin(); //this isnt working properly, gives black screen even tho objects should still be visible, no idea
            //dest-current position is the direction, dest is origin so just negate
        else
            //instead of origin pick a random point LANDER_PAPP_DISTANCE away from the asteroid
            direction = Service::getPointOnSphere(Service::getRandFloat(0,360), Service::getRandFloat(0,360), passDistance)-transform->getOrigin();

        rigidbody->setLinearVelocity(direction.normalize()*initialSpeed); //start box falling towards asteroid
    }


    void addImpulseToLanderQueue(float duration, float x, float y, float z, bool torque){
        landerBoostQueueLock.lock();
        landerBoostQueue.push_back(LanderBoostCommand{duration, glm::vec3{x,y,z}, torque});
        landerBoostQueueLock.unlock();
    }

    bool landerImpulseRequested(){
        landerBoostQueueLock.lock();
        bool empty = landerBoostQueue.empty();
        landerBoostQueueLock.unlock();
        return !empty;
    }

    LanderBoostCommand& popLanderImpulseQueue(){
        landerBoostQueueLock.lock();
        LanderBoostCommand& command = landerBoostQueue.front();
        landerBoostQueue.pop_front();
        landerBoostQueueLock.unlock();
        return command;
    }

    void landerCollided(){
        ai.setAutopilot(false);
    }

    void landerSetAutopilot(bool b){
        ai.setAutopilot(b);
    }

    ~LanderObj(){};
};