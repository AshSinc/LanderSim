#pragma once
#include "obj_collisionRender.h"
#include <glm/gtx/fast_square_root.hpp>
#include "sv_randoms.h"

struct LanderObj : virtual CollisionRenderObj{
    bool collisionCourse = false;
    bool randomStartPositions = false;
    float asteroidGravForceMultiplier = 0.5f;
    float startDistance = 150.0f;
    float passDistance = 60.0f;
    float initialSpeed = 1.5f;

    float landerVelocity = 0.0f;
    float gravitationalForce = 0.0f;

    void init(btAlignedObjectArray<btCollisionShape*>* collisionShapes, btDiscreteDynamicsWorld* dynamicsWorld, Mediator& r_mediator){
        //create a dynamic rigidbody
        btCollisionShape* collisionShape = new btBoxShape(btVector3(1,1,1));
        //colShape->setMargin(0.05);
        collisionShape->setLocalScaling(btVector3(scale.x, scale.y, scale.z)); //may not be 1:1 scale between bullet and vulkan
        collisionShapes->push_back(collisionShape);

        /// create lander transform
        btTransform transform;
        transform.setIdentity();

        initTransform(&transform);

        btScalar btMass(mass);

        //rigidbody is dynamic if and only if mass is non zero, otherwise static
        bool isDynamic = (btMass != 0.f);
        btVector3 localInertia(0, 0, 0);
        if (isDynamic)
            collisionShape->calculateLocalInertia(btMass, localInertia);

        //using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
        btDefaultMotionState* myMotionState = new btDefaultMotionState(transform);
        btRigidBody::btRigidBodyConstructionInfo rbInfo(btMass, myMotionState, collisionShape, localInertia);
        rbInfo.m_friction = 2.0f;
        rbInfo.m_spinningFriction  = 0.25f;
        //rbInfo.m_rollingFriction = 0.5f;
        btRigidBody* rigidbody = new btRigidBody(rbInfo);
        rigidbody->setActivationState(DISABLE_DEACTIVATION); //stop body from disabling collision, bullet threshholds are pretty loose

        initRigidBody(&transform, rigidbody);

        dynamicsWorld->addRigidBody(rigidbody);
    }

    void timestepBehaviour(btRigidBody* body){
        btVector3 direction = -btVector3(pos.x , pos.y, pos.z); //asteroid is always at origin so dir of gravity is aways towards 0
        gravitationalForce = asteroidGravForceMultiplier*glm::fastInverseSqrt(direction.distance(btVector3(0,0,0)));
        body->setGravity(direction.normalize()*gravitationalForce);
        //will also need a local velocity, subtracting the asteroid rotational velocity, maths... ???
        landerVelocity = body->getLinearVelocity().length(); //will need to properly work out the world size 
    }

    void updateWorldStats(WorldStats* worldStats){
        //stored for retrival in UI with getGravitationalForce()
        worldStats->landerVelocity = landerVelocity;
        worldStats->gravitationalForce = gravitationalForce;
    }

    void initTransform(btTransform* transform){
        if(randomStartPositions){
            transform->setOrigin(Service::getPointOnSphere(Service::getRandFloat(0,360), Service::getRandFloat(0,360), startDistance));
        }
        else{
            transform->setOrigin(btVector3(pos.x, pos.y, pos.z));
        }
    }

    void initRigidBody(btTransform* transform, btRigidBody* rigidbody){
        btVector3 direction;
        if(collisionCourse || !randomStartPositions)
            //dest-current position is the direction, dest is origin so just negate
            direction = -transform->getOrigin();
        else
            //instead of origin pick a random point LANDER_START_DISTANCE away from the asteroid
            direction = Service::getPointOnSphere(Service::getRandFloat(0,360), Service::getRandFloat(0,360), passDistance)-transform->getOrigin();

        rigidbody->setLinearVelocity(direction.normalize()*initialSpeed); //start box falling towards asteroid
    }

    ~LanderObj(){};
};