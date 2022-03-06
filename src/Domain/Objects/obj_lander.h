#pragma once
#include "obj_collisionRender.h"
#include <glm/gtx/fast_square_root.hpp>
#include "sv_randoms.h"
#include "lander_cpu.h"
#include <deque>
#include <mutex>
#include "obj_spotLight.h"

struct LanderObj : virtual CollisionRenderObj{ //this should impliment an interface for 
    Lander::CPU cpu = Lander::CPU();
    //bool collisionCourse = false;
    //bool randomStartPositions;
    double asteroidGravForceMultiplier;
    float startDistance;
    //float passDistance = 70.0f;
    float initialSpeed = 0.00001f; //if 0 we get a black screen on auto camera, lander is in correct pos though, changing focus fixes
    btVector3 asteroidRotationalVelocity = btVector3(0,0,0);
    
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

    //btQuaternion initialRotation;
    
    void init(btAlignedObjectArray<btCollisionShape*>* collisionShapes, btDiscreteDynamicsWorld* dynamicsWorld, Mediator& r_mediator){
        //colShape->setMargin(0.05);
        //collisionShape->setLocalScaling(btVector3(scale.x, scale.y, scale.z)); //may not be 1:1 scale between bullet and vulkan
        //collisionShapes->push_back(collisionShape);

        //we will make a compound shape, to keep it simple a box for each foot, and a cylinder for the body
        btCompoundShape* compoundShape = new btCompoundShape();

        collisionShapes->push_back(compoundShape); //we are tracking collisions shapes to delete them on close

        float bodyRadius = 1;
        float bodyHeight = 0.3f;

        // create two shapes for the rod and the load
        btCollisionShape* body = new btCylinderShapeZ(btVector3(bodyRadius, bodyRadius, bodyHeight/2)); //havent really tested these values
        btCollisionShape* foot = new btBoxShape(btVector3(0.1f, 0.1f, 0.1f));
        // create a transform we'll use to set each object's position 
        btTransform transform;

        transform.setIdentity();
        transform.setOrigin(btVector3(0, 0, 0.2f)); //offset the body up, not tested
        
        // add the body
        compoundShape->addChildShape(transform, body);
        //set first foot offset position
        transform.setOrigin(btVector3(-1.1, -0.63f, -0.53f));
        //add foot 1
        compoundShape->addChildShape(transform, foot);
        //set second foot offset position
        transform.setOrigin(btVector3(0.005, 1.26, -0.53f));
        //add foot 2
        compoundShape->addChildShape(transform, foot);
        //set third foot offset position
        transform.setOrigin(btVector3(1.09, -0.63, -0.53f));
        //add foot 3
        compoundShape->addChildShape(transform, foot);

        //create lander transform
        //btTransform transform;
        landerTransform.setIdentity();

        landerTransform.setOrigin(btVector3(pos.x, pos.y, pos.z));
        
        btQuaternion quat;
        quat.setEulerZYX(0,0,0);

        //ISSUE - Need to set initial rotation pointing at asteroid
        //glm::quat rot = glm::lookAt(pos, glm::vec3(0), glm::vec3(0,0,1));
        //landerTransform.setRotation(Service::glmToBulletQ(rot));
        landerTransform.setRotation(quat);

        btScalar btMass(mass);

        //rigidbody is dynamic if and only if mass is non zero, otherwise static
        bool isDynamic = (btMass != 0.f);
        btVector3 localInertia(0, 0, 0);
        if (isDynamic)
            compoundShape->calculateLocalInertia(btMass, localInertia);

        //using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
        btDefaultMotionState* myMotionState = new btDefaultMotionState(landerTransform);
        btRigidBody::btRigidBodyConstructionInfo rbInfo(btMass, myMotionState, compoundShape, localInertia);
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
        up = glm::normalize(Service::bt2glm(landerWorldTransform(btVector3{0,0,1})));
        forward = glm::normalize(Service::bt2glm(landerWorldTransform(btVector3{1,0,0})));
    }

    void timestepBehaviour(btRigidBody* body, float timeStep){
        //set the gravity for the lander towards the asteroid
        btVector3 direction = -btVector3(pos.x, pos.y, pos.z); //asteroid is always at origin so dir of gravity is aways towards 0
        gravitationalForce = asteroidGravForceMultiplier*glm::fastInverseSqrt(direction.distance(btVector3(0,0,0)));
        landerGravityVector = direction.normalize()*gravitationalForce;
        body->setGravity(landerGravityVector);
        
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
    void updateSpotlight(){
        p_spotlight->pos = glm::vec4(pos, 1.0f) + (rot * glm::vec4(p_spotlight->initialPos, 1.0f)); //we set the light position to that of the initalPos + lander current position
        p_spotlight->direction = rot * glm::vec4(p_spotlight->initialPos, 0.0f); //we are using initialPos of the light as a direction, * landers rotation
    }

    void updateWorldStats(WorldStats* worldStats){
        //stored for retrival in UI with getGravitationalForce()
        worldStats->landerVelocity = landerVelocity;
        worldStats->gravitationalForce = gravitationalForce;
    }

    void initRigidBody(btTransform* transform, btRigidBody* rigidbody){
        //this isnt working properly, gives black screen, although everything is still in correct position, black screen caused with auto camera, and maybe transform isnt set yet
        //btVector3 direction = -transform->getOrigin(); 

        //btVector3 direction = btVector3(0,0,-1); //lets do this for now, this causes black screen as well!
        btVector3 direction = Service::getPointOnSphere(Service::getRandFloat(0,360), Service::getRandFloat(0,360), 100)-transform->getOrigin(); //why on earth does this work instead
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