#pragma once
#include "obj_collisionRender.h"
#include <glm/gtx/fast_square_root.hpp>
#include "sv_randoms.h"
#include "lander_cpu.h"
#include <deque>
#include <mutex>
#include "obj_spotLight.h"

struct LanderObj : virtual CollisionRenderObj{ //this should impliment an interface for 
    //Ai::LanderAi ai = Ai::LanderAi();
    Lander::CPU cpu = Lander::CPU();
    bool collisionCourse = false;
    bool randomStartPositions = false;
    double asteroidGravForceMultiplier = 0.01f;
    float startDistance = 150.0f;
    float passDistance = 70.0f;
    float initialSpeed = 1.5f; //this var isnt actually set here, its in data_scene.h
    btVector3 asteroidRotationalVelocity = btVector3(0,0,0); //we can cheat by assigning it in myscene.cpp
    

    float landerVelocity = 0.0f;
    glm::vec3 landerVelocityVector = glm::vec3(0.0f);
    glm::vec3 landerAngularVelocity = glm::vec3(0.0f);
    
    float gravitationalForce = 0.0f;
    btVector3 landerGravityVector = btVector3(0,0,0);

    const float BOOST_STRENGTH = 1.0f;
    const float LANDER_BOOST_CAP = 10.0f; //physical limit for an individual boost, regardless of requested boost 
    float landerRotationalBoostStrength = 1.0f;

    btTransform landerTransform;
    Mediator* p_mediator;
    WorldSpotLightObject* p_spotlight; 
    

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
        cpu.init(p_mediator, this);
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
        gravitationalForce = asteroidGravForceMultiplier*glm::fastInverseSqrt(direction.distance(btVector3(0,0,0)));
        landerGravityVector = direction.normalize()*gravitationalForce;
        body->setGravity(landerGravityVector);
        
        //btVector3 direction = btVector3(0,0,-1); //
        //gravitationalForce = 0.1f;
        //landerGravityVector = direction.normalize()*gravitationalForce;
        //body->setGravity(landerGravityVector);
        
        //store the linear velocity, 
        landerVelocity = body->getLinearVelocity().length(); //will need to properly work out the world size 

        landerVelocityVector = Service::bt2glm(body->getLinearVelocity());

        landerAngularVelocity = Service::bt2glm(body->getAngularVelocity());

        //will probably need to store actual velocity vector as well 

        updateLanderUpForward(body);

        cpu.simulationTick(body, timeStep); //let the ai have a tick    
    }

    //this will be called from world_physics directly from now on, little hacky, need to derive new class from WorldPhysics
    //and seperate out this call and the update landing site calls
    //ISSUE, when rotating thelight pos or direction is not right?
    void updateSpotlight(){
        //rotated pos or direction not correct
        p_spotlight->pos = glm::vec4(pos, 1.0f) + (rot * glm::vec4(p_spotlight->initialPos, 1.0f)); //we set the light position to that of the initalPos + lander current position
        p_spotlight->direction = rot * glm::vec4(p_spotlight->initialPos, 0.0f); //we are using initialPos of the light as a direction, * landers rotation
        //std::cout << glm::to_string(p_spotlight->direction) << " light dir\n";
    }

    void updateWorldStats(WorldStats* worldStats){
        //stored for retrival in UI with getGravitationalForce()
        worldStats->landerVelocity = landerVelocity;
        worldStats->gravitationalForce = gravitationalForce;
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

    void landerCollided(){
        cpu.setAutopilot(false);
        cpu.setImaging(false);
    }

    void landerSetAutopilot(bool b){
        cpu.setAutopilot(b);
    }

    ~LanderObj(){};
};