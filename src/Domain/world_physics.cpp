#include "world_physics.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <glm/gtx/fast_square_root.hpp>
#include "vk_renderer.h"
#include "obj_collisionRender.h"
#include "mediator.h"
#include <BulletCollision/Gimpact/btGImpactShape.h>
#include <BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h>
//#include <glm/gtc/quaternion.hpp>
//#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>


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
        //temporary, should have bindable WorldObject property in WorldObject, so u can bind its movement to another object 
        //then check for a binding on movement
        updateLandingSiteObjects(); 
        checkCollisions();
    }
}

void WorldPhysics::updateLandingSiteObjects(){
    //get LandingSite WorldObject too and update that first
    CollisionRenderObj asteroidRenderObject = *p_collisionObjects->at(1);//asteroid will be 1 , lander is 0
    WorldObject& landingSite = r_mediator.scene_getFocusableObject("Landing_Site");
    glm::vec3 rotated_point = asteroidRenderObject.rot * glm::vec4(landingSite.initialPos, 1);
    landingSite.pos = rotated_point;
    landingSite.rot = asteroidRenderObject.rot*landingSite.initialRot;

    for(int x = 0; x < 4; x++){
        WorldObject& box = r_mediator.scene_getWorldObject(4+x); //4 is the first index of landing box in the world object list
        glm::vec3 directionToOrigin = glm::normalize(-landingSite.initialPos);
        glm::vec3 rotated_point = asteroidRenderObject.rot * glm::vec4(box.initialPos, 1);
        box.pos = rotated_point;
        box.rot = asteroidRenderObject.rot*box.initialRot;
    }

    //store the up vector (for taking images and aligning)
    glm::mat4 scale = glm::scale(glm::mat4{ 1.0 }, landingSite.scale);
    glm::mat4 translation = glm::translate(glm::mat4{ 1.0 }, landingSite.pos);
    glm::mat4 rotation = landingSite.rot;
    glm::mat4 landerWorldTransform = translation * rotation * scale;
    int indexUpAxis = 1;
    landingSite.up = landerWorldTransform[1];
    int indexFwdAxis = 0;
    landingSite.forward = landerWorldTransform[0];    
}

//this function is just to help find locations for the landing site, manually move it in sim, update the render object position or rotation and print out the pos and rot
void WorldPhysics::moveLandingSite(float x, float y, float z, bool torque){
    WorldObject& landingSite = r_mediator.scene_getFocusableObject("Landing_Site");

    if(torque){
        //input 
        float step = 1.0f;
        landingSite.yaw+=x*step;
        landingSite.pitch+=y*step;
        landingSite.roll+=z*step;

        landingSite.initialRot = glm::yawPitchRoll(glm::radians(landingSite.yaw), glm::radians(landingSite.pitch), glm::radians(landingSite.roll));

        for(int x = 0; x < 4; x++){
            WorldObject& box = r_mediator.scene_getWorldObject(4+x); //4 is the first index of landing box in the world object list
            box.initialRot = landingSite.initialRot;

            glm::vec3 landingBoxPos;
            switch (x){
                case 0:landingBoxPos = landingSite.pos+glm::vec3(-1,-1,0);break;
                case 1:landingBoxPos = landingSite.pos+glm::vec3(1,-1,0);break;
                case 2:landingBoxPos = landingSite.pos+glm::vec3(-1,1,0);break;
                case 3:landingBoxPos = landingSite.pos+glm::vec3(1,1,0);break;
                default:break;
            }

            //we need to account for landing site rotation before getting our final box position
            //we start with our offset landingBoxPos, make a translation matrix of our landingBoxPos, and the inverse of it
            //then we construct the whole matrix, (remember to read in reverse order)
            //ie we move the point back to 0,0,0 (-landingSitePos), rotate it around origin by landingSiteRot, then translate back to our starting position (+landingSitePos)
            //also used in dmn_myScene, could move to Service/helper functions
            glm::mat4 translate = glm::translate(glm::mat4(1), landingSite.pos);
            glm::mat4 invTranslate = glm::inverse(translate);
            glm::mat4 objectRotationTransform = translate * landingSite.initialRot * invTranslate;
            //box.pos = objectRotationTransform*glm::vec4(landingBoxPos, 1);
            box.initialPos = objectRotationTransform*glm::vec4(landingBoxPos, 1);
        }
    }
    else{
        glm::vec3 correctedDirection = landingSite.rot * glm::vec4(x, y, z, 0);
        landingSite.pos += correctedDirection/10.0f;
        landingSite.initialPos = landingSite.pos;
        for(int x = 0; x < 4; x++){
            WorldObject& box = r_mediator.scene_getWorldObject(4+x); //4 is the first index of landing box in the world object list
            box.pos += correctedDirection/10.0f;
            box.initialPos = box.pos;
        }
    } 

    std::cout << "Landing Site Moved: " << "\n";
    std::cout << glm::to_string(landingSite.initialPos) << "\n";
    std::cout << landingSite.yaw  << ", " << landingSite.pitch << ", " << landingSite.roll << "\n";
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

void WorldPhysics::addImpulseToLanderQueue(float duration, float x, float y, float z, bool torque){
    landerBoostQueueLock.lock();
    landerBoostQueue.push_back(LanderBoostCommand{duration, glm::vec3{x,y,z}, torque});
    landerBoostQueueLock.unlock();
    
    //if(outputActionToFile){

    //}
}

bool WorldPhysics::landerImpulseRequested(){
    landerBoostQueueLock.lock();
    bool empty = landerBoostQueue.empty();
    landerBoostQueueLock.unlock();
    return !empty;
}

LanderBoostCommand& WorldPhysics::popLanderImpulseQueue(){
    landerBoostQueueLock.lock();
    LanderBoostCommand& command = landerBoostQueue.front();
    landerBoostQueue.pop_front();
    landerBoostQueueLock.unlock();
    return command;
}

//instanciate the world space
//adds default objects to scene
WorldPhysics::WorldPhysics(Mediator& mediator): r_mediator{mediator}{
    landerBoostQueue = std::deque<LanderBoostCommand>(0);
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