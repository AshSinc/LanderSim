#include "world_physics.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <random>
#include <iomanip> //used for setprecision in random function
#include <glm/gtx/fast_square_root.hpp>
#include "vk_renderer.h"
#include "obj_collisionRender.h"
#include "mediator.h"

void WorldPhysics::updateDeltaTime(){
    auto now = std::chrono::high_resolution_clock::now();
    deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(now - lastTime).count();
    lastTime = now;
}

//need a sync object here, semaphore 
void WorldPhysics::setSimSpeedMultiplier(float multiplier){
    worldStats.timeStepMultiplier = multiplier;
}

double WorldPhysics::deltaTime = 0.0;

void WorldPhysics::worldTick(){
	///-----stepsimulation_start-----
    if(worldStats.timeStepMultiplier != 0){ //if we are not paused
        //ISSUE
        //timesteps and maxsubsteps are likely not accurate
        //SOLUTION
        //understand how bullet handles this first, and find examples
        float fixedTimeStep = 0.01666666754F;
        float timeStep = deltaTime*worldStats.timeStepMultiplier;
        int maxSubSteps = timeStep/fixedTimeStep + SUBSTEP_SAFETY_MARGIN;
        std::cout << "Timestep: "<< timeStep << "\n";
        std::cout << "Max substeps: "<< maxSubSteps << "\n";
        dynamicsWorld->stepSimulation(timeStep, maxSubSteps, fixedTimeStep);

        //update positions of world objects from similation transforms
        for (int j = dynamicsWorld->getNumCollisionObjects() - 1; j >= 0; j--){
            btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[j];
            btRigidBody* body = btRigidBody::upcast(obj);
            btTransform trans;
            if (body && body->getMotionState()){
                body->getMotionState()->getWorldTransform(trans);
            }
            else{
                trans = obj->getWorldTransform();
            }
            //update positions of objects, could clean, need to id objects properly across the project
            //statically referenced everywhere and will break when objects added or removed
            btVector3 transform = trans.getOrigin();
            btQuaternion rotation = trans.getRotation();
            glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f),rotation.getAngle(),glm::vec3(rotation.getAxis().getX(), rotation.getAxis().getY(), rotation.getAxis().getZ()));
            if(j==0){
               // objects[4].rot = rotationMatrix;
              //  objects[4].pos = glm::vec3(float(transform.getX()), float(transform.getY()), float(transform.getZ()));
            }
            if(j==1){
               // objects[2].rot = rotationMatrix;
               // objects[2].pos = glm::vec3(float(transform.getX()), float(transform.getY()), float(transform.getZ()));
                
                //each tick we will set the gravity for the lander to be in the direction of the asteroid, reduced by distance
                btVector3 direction = -btVector3(transform.getX(),transform.getY(),transform.getZ());
                
                //stored for retrival in UI with getGravitationalForce()
                worldStats.gravitationalForce = ASTEROID_GRAVITATIONAL_FORCE*glm::fastInverseSqrt(direction.distance(btVector3(0,0,0)));
                
                body->setGravity(direction.normalize()*worldStats.gravitationalForce);

                //will also need a local velocity, subtracting the asteroid rotational velocity, maths...
                worldStats.landerVelocity = body->getLinearVelocity().length(); //will need to properly work out the world size 
            }
        }

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
}

//needs a semaphore or sync protection
WorldPhysics::WorldStats& WorldPhysics::getWorldStats(){
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

void WorldPhysics::loadCollisionMesh(CollisionRenderObj* collisionObject){ 
    if(collisionObject->boxShape){
        //create a dynamic rigidbody
        btCollisionShape* collisionShape = new btBoxShape(btVector3(1,1,1));
        //colShape->setMargin(0.05);
        collisionShape->setLocalScaling(btVector3(collisionObject->scale.x, collisionObject->scale.y, collisionObject->scale.z)); //may not be 1:1 scale between bullet and vulkan
        collisionShapes.push_back(collisionShape);

        /// create lander transform
        btTransform transform;
        transform.setIdentity();
        if(RANDOMIZE_START){
            transform.setOrigin(getPointOnSphere(getRandFloat(0,360), getRandFloat(0,360), LANDER_START_DISTANCE));
        }
        else
            transform.setOrigin(btVector3(collisionObject->pos.x, collisionObject->pos.y, collisionObject->pos.z));

        btScalar mass(collisionObject->mass);

        //rigidbody is dynamic if and only if mass is non zero, otherwise static
        bool isDynamic = (mass != 0.f);
        btVector3 localInertia(0, 0, 0);
        if (isDynamic)
            collisionShape->calculateLocalInertia(mass, localInertia);

        //using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
        btDefaultMotionState* myMotionState = new btDefaultMotionState(transform);
        btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, collisionShape, localInertia);
        rbInfo.m_friction = 2.0f;
        //rbInfo.m_spinningFriction  = 0.5f;
        //rbInfo.m_rollingFriction = 0.5f;
        btRigidBody* rigidbody = new btRigidBody(rbInfo);
        rigidbody->setActivationState(DISABLE_DEACTIVATION); //stop body from disabling collision, bullet threshholds are pretty loose

        //calculate initial direction of travel
        btVector3 direction;
        if(LANDER_COLLISION_COURSE || !RANDOMIZE_START)
            //dest-current position is the direction, dest is origin so just negate
            direction = -transform.getOrigin();
        else
            //instead of origin pick a random point LANDER_START_DISTANCE away from the asteroid
            direction = getPointOnSphere(getRandFloat(0,360), getRandFloat(0,360), LANDER_PASS_DISTANCE)-transform.getOrigin();

        rigidbody->setLinearVelocity(direction.normalize()*INITIAL_LANDER_SPEED); //start box falling towards asteroid

        dynamicsWorld->addRigidBody(rigidbody);
    }
    else if(collisionObject->concaveTriangleShape){
        int base = collisionObject->indexBase;
        int count = collisionObject->indexCount;
        std::vector<Vertex>& allV = r_mediator.renderer_getAllVertices(); //reference all loaded model vertices
        std::vector<uint32_t>& allI = r_mediator.renderer_getAllIndices(); //reference all loaded model indices

        btTriangleMesh *mTriMesh = new btTriangleMesh(true, false);

        //create a trimesh by stepping through allVertices using allIndices to select the correct vertices
        for (int i = base; i < base+count; i+=3) {
            btVector3 v1(allV[allI[i]].pos.x, allV[allI[i]].pos.y, allV[allI[i]].pos.z);
            btVector3 v2(allV[allI[i+1]].pos.x, allV[allI[i+1]].pos.y, allV[allI[i+1]].pos.z);
            btVector3 v3(allV[allI[i+2]].pos.x, allV[allI[i+2]].pos.y, allV[allI[i+2]].pos.z);
            mTriMesh->addTriangle(v1, v2, v3, true);
        }
        btGImpactMeshShape *collisionShape = new btGImpactMeshShape(mTriMesh);
        //asteroidCollisionShape->setMargin(0.05);
        collisionShape->setLocalScaling(btVector3(collisionObject->scale.x, collisionObject->scale.y, collisionObject->scale.z)); //may not be 1:1 scale between bullet and vulkan
        collisionShape->updateBound();// Call this method once before doing collisions 
        collisionShapes.push_back(collisionShape);

        btTransform transform;
        transform.setIdentity();
        transform.setOrigin(btVector3(collisionObject->pos.x, collisionObject->pos.y, collisionObject->pos.z));

        //set initial rotation of asteroid
        btQuaternion quat;
        if(RANDOMIZE_START)
            quat.setEulerZYX(getRandFloat(0,359),getRandFloat(0,359),getRandFloat(0,359));
        else
            quat.setEulerZYX(0,0,0);
        transform.setRotation(quat);

        btScalar mass(collisionObject->mass);
        //rigidbody is dynamic if and only if mass is non zero, otherwise static
        bool isDynamic = (mass != 0.f);
        btVector3 localInertia(0, 0, 0);
        if (isDynamic)
            collisionShape->calculateLocalInertia(mass, localInertia);

        //using motionstate is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
        btDefaultMotionState* myMotionState = new btDefaultMotionState(transform);
        btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, collisionShape, localInertia);
        rbInfo.m_friction = 5.0f;
        btRigidBody* rigidbody = new btRigidBody(rbInfo);
        rigidbody->setActivationState(DISABLE_DEACTIVATION); //stop body from disabling collision, bullet threshholds are pretty loose

        //set initial rotational velocity of asteroid
        if(RANDOMIZE_START){
            float f = MAX_ASTEROID_ROTATION_VELOCITY;
            rigidbody->setAngularVelocity(btVector3(getRandFloat(-f,f),getRandFloat(-f,f),getRandFloat(-f,f))); //random rotation of asteroid
        }
        else
            rigidbody->setAngularVelocity(btVector3(0.005f, 0.015f, 0.01f)); //initial rotation of asteroid
        dynamicsWorld->addRigidBody(rigidbody);//add the body to the dynamics world
    }
}

//void WorldPhysics::loadCollisionMeshes(Vk::Renderer& engine){ 
void WorldPhysics::loadCollisionMeshes(std::vector<std::shared_ptr<CollisionRenderObj>>& collisionObjects){ 
    for (std::shared_ptr<CollisionRenderObj> obj : collisionObjects){
        loadCollisionMesh(obj.get());
    }
}

//helper, gets random btvector between min and max
float WorldPhysics::getRandFloat(float min, float max){
    float offset = 0.0f;
    if(min < 0){
        offset = -min;
        min = 0;
        max += offset;
    }
    std::random_device rd;
    std::default_random_engine eng(rd());
    std::uniform_real_distribution<> distr(min, max);
    std::setprecision(10);
    float result = distr(eng)-offset;
    std::cout << result << "\n";
    return result;
}

//helper, gets point on sphere of given radius, origin 0
btVector3 WorldPhysics::getPointOnSphere(float pitch, float yaw, float radius){
    btVector3 result;
    result.setX(radius * cos(glm::radians(yaw)) * sin(glm::radians(pitch)));
    result.setY(radius * sin(glm::radians(yaw)) * sin(glm::radians(pitch)));
    result.setZ(radius * cos(glm::radians(pitch)));
    return result;
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
            else if(selectedSimSpeedIndex > 6) //need to add a var for array init so we dont have a hardcoded size here
                selectedSimSpeedIndex = 6;
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

void WorldPhysics::cleanupBullet(){
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
	delete dynamicsWorld;
	delete solver;
	//delete broadphase
	delete overlappingPairCache;
	delete dispatcher;
	delete collisionConfiguration;
	//next line is optional: it will be cleared by the destructor when the array goes out of scope
	collisionShapes.clear();
}