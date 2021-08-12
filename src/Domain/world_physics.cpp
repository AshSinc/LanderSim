#include "world_physics.h"
#include <iostream>
#include <chrono>
#include <thread>
//#include <random>
//#include <iomanip> //used for setprecision in random function
#include <glm/gtx/fast_square_root.hpp>
#include "vk_renderer.h"
#include "obj_collisionRender.h"
#include "mediator.h"
#include <BulletCollision/Gimpact/btGImpactShape.h>
#include <BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h>

void WorldPhysics::updateDeltaTime(){
    auto now = std::chrono::high_resolution_clock::now();
    deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(now - lastTime).count();
    lastTime = now;
}

//need a sync object here, semaphore 
void WorldPhysics::setSimSpeedMultiplier(float multiplier){
    worldStats.timeStepMultiplier = multiplier;
}

void WorldPhysics::worldTick(){
    if(worldStats.timeStepMultiplier != 0){ //if we are not paused
        //ISSUE
        //timesteps and maxsubsteps are likely not accurate
        //SOLUTION
        //understand how bullet handles this first, and find examples
        float fixedTimeStep = 0.01666666754F;
        float timeStep = deltaTime*worldStats.timeStepMultiplier;
        int maxSubSteps = timeStep/fixedTimeStep + SUBSTEP_SAFETY_MARGIN;
        //std::cout << "Timestep: "<< timeStep << "\n";
        //std::cout << "Max substeps: "<< maxSubSteps << "\n";
        dynamicsWorld->stepSimulation(timeStep, maxSubSteps, fixedTimeStep);

        updateCollisionObjects();
        checkCollisions();
    }
}

void WorldPhysics::updateCollisionObjects(){
    //update positions of world objects from similation transforms
    for(std::shared_ptr<CollisionRenderObj> collisionRenderObj : *p_collisionObjects){
        btCollisionObject* obj = collisionRenderObj->p_btCollisionObject;
        btRigidBody* body = btRigidBody::upcast(obj);
        btTransform transform = obj->getWorldTransform();
        btVector3 origin = transform.getOrigin();
        btQuaternion rotation = transform.getRotation();
        glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f),rotation.getAngle(),glm::vec3(rotation.getAxis().getX(), rotation.getAxis().getY(), rotation.getAxis().getZ()));

        collisionRenderObj->rot = rotationMatrix;
        collisionRenderObj->pos = glm::vec3(float(origin.getX()), float(origin.getY()), float(origin.getZ()));

        collisionRenderObj->timestepBehaviour(body);
        collisionRenderObj->updateWorldStats(&worldStats);
    }
}

void WorldPhysics::checkCollisions(){
    //check for collisions active and do something (start a timer, compare velocities over time, if minimal motion then end sim state)
    //get number of overlapping manifolds and iterate over them
    int numManifolds = dynamicsWorld->getDispatcher()->getNumManifolds();
    for (int i = 0; i < numManifolds; i++){
        btPersistentManifold * contactManifold = dynamicsWorld->getDispatcher()->getManifoldByIndexInternal(i);
        //get number of contact points and iterate over them
        int numContacts = contactManifold->getNumContacts();
        if (numContacts > 0){
            btScalar totalImpact = 0.f;
            for (int p = 0; p < contactManifold->getNumContacts(); p++){
                totalImpact += contactManifold->getContactPoint(p).getAppliedImpulse();
            }

            //ISSUE
            //totalImpact should be scaled by simulation time (deltaTime), but 
            //Likely related to abode ISSUE of timesteps
            //SOLUTION
            //Fix timesteps first, finda  way to test

            totalImpact*=deltaTime;
            std::cout << "Collision with ImpactForce : " << totalImpact << "\n";

            if(totalImpact > 0){
                worldStats.lastImpactForce = totalImpact;
                if(worldStats.lastImpactForce > worldStats.largestImpactForce)
                    worldStats.largestImpactForce = worldStats.lastImpactForce;

                if (totalImpact > 5.f)
                    std::cout << "Lander Destroyed\n";
            }
        }
    }
}   

//needs a semaphore or sync protection
WorldStats& WorldPhysics::getWorldStats(){
    return worldStats;
}

void WorldPhysics::mainLoop(){
    updateDeltaTime();
    worldTick();
}

//initialize bullet physics engine
void WorldPhysics::initBullet(){
    ///collision configuration contains default setup for memory, collision setup. Advanced users can create their own configuration.
    collisionConfiguration = new btDefaultCollisionConfiguration();
    dispatcher = new btCollisionDispatcher(collisionConfiguration);
    btGImpactCollisionAlgorithm::registerAlgorithm(dispatcher);
    overlappingPairCache = new btDbvtBroadphase();
    //overlappingPairCache = new btSimpleBroadphase();
    ///the default constraint solver. For parallel processing you can use a different solver (see Extras/BulletMultiThreaded)
    solver = new btSequentialImpulseConstraintSolver();
    dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, overlappingPairCache, solver, collisionConfiguration);
    dynamicsWorld->setGravity(btVector3(0, 0, 0));
}

void WorldPhysics::loadCollisionMeshes(std::vector<std::shared_ptr<CollisionRenderObj>>* collisionObjects){ 
    p_collisionObjects = collisionObjects;
    collisionShapes.clear();
    int i = 0;
    for (std::shared_ptr<CollisionRenderObj> obj : *p_collisionObjects){
        obj->init(&collisionShapes, dynamicsWorld, r_mediator);
        obj->p_btCollisionObject = dynamicsWorld->getCollisionObjectArray()[i++];  //add the btCollisionObject pointer to the object, se we can iterate through this to better link the objects
    }
}

void WorldPhysics::changeSimSpeed(int direction, bool pause){
    if(pause){ //toggle pause on and off
        if(getWorldStats().timeStepMultiplier == 0)
            setSimSpeedMultiplier(SIM_SPEEDS[selectedSimSpeedIndex]);
        else
            setSimSpeedMultiplier(0);
    }
    else{
        if(direction == 0) //0 will be a normal speed shortcut eventually?
            selectedSimSpeedIndex = 2;
        else{
            selectedSimSpeedIndex+=direction;
            if(selectedSimSpeedIndex < 0)
                selectedSimSpeedIndex = 0;
            else if(selectedSimSpeedIndex > SPEED_ARRAY_SIZE)
                selectedSimSpeedIndex = SPEED_ARRAY_SIZE;
            setSimSpeedMultiplier(SIM_SPEEDS[selectedSimSpeedIndex]);
        }
    }
}

//instanciate the world space
//adds default objects to scene
WorldPhysics::WorldPhysics(Mediator& mediator): r_mediator{mediator}{
    initBullet();
    updateDeltaTime();
}

WorldPhysics::~WorldPhysics(){
    cleanupBullet();
}

void WorldPhysics::reset(){
    //set any variables back to default (user might load a new scene)
    //changeSimSpeed(0, false); //pause and sim speed should be seperated
    selectedSimSpeedIndex = 2;
    setSimSpeedMultiplier(SIM_SPEEDS[selectedSimSpeedIndex]);
    //cleanup in the reverse order of creation/initialization
	///-----cleanup_start-----
	//remove the rigidbodies from the dynamics world and delete them
	for (int i = dynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i--){
		btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];
		btRigidBody* body = btRigidBody::upcast(obj);
		if (body && body->getMotionState())
		{
			delete body->getMotionState();
		}
		dynamicsWorld->removeCollisionObject(obj);
		delete obj;
	}
	//delete collision shapes
    for (int j = 0; j < collisionShapes.size(); j++){
		btCollisionShape* shape = collisionShapes[j];
		collisionShapes[j] = 0;
		delete shape;
	}
}

void WorldPhysics::cleanupBullet(){
    reset();
	delete dynamicsWorld;
	delete solver;
	//delete broadphase
	delete overlappingPairCache;
	delete dispatcher;
	delete collisionConfiguration;
	//next line is optional: it will be cleared by the destructor when the array goes out of scope
	collisionShapes.clear();
}