#include <world_state.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <random>
#include <iomanip> //used for setprecision in random function
#include <glm/gtx/fast_square_root.hpp>
#include "vk_engine.h"
#include "world_input.h"

void WorldState::addObject(std::vector<WorldObject>& container, WorldObject obj){
    container.push_back(obj);
}

WorldState* WorldState::getWorld(){
    return this;
}

std::vector<WorldObject>& WorldState::getWorldObjectsRef(){
    return objects;
}

std::vector<WorldPointLightObject>& WorldState::getWorldPointLightObjects(){
    return pointLights;
}

std::vector<WorldSpotLightObject>& WorldState::getWorldSpotLightObjects(){
    return spotLights;
}

void WorldState::updateDeltaTime(){
    auto now = std::chrono::high_resolution_clock::now();
    deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(now - lastTime).count();
    lastTime = now;
}

//need a sync object here, semaphore 
void WorldState::setSimSpeedMultiplier(float multiplier){
    worldStats.timeStepMultiplier = multiplier;
}

double WorldState::deltaTime = 0.0;

void WorldState::worldTick(){
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
                objects[4].rot = rotationMatrix;
                objects[4].pos = glm::vec3(float(transform.getX()), float(transform.getY()), float(transform.getZ()));
            }
            if(j==1){
                objects[2].rot = rotationMatrix;
                objects[2].pos = glm::vec3(float(transform.getX()), float(transform.getY()), float(transform.getZ()));
                
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
WorldState::WorldStats WorldState::getWorldStats(){
    return worldStats;
}

void WorldState::mainLoop(){
    updateDeltaTime();
    worldTick();
}

void WorldState::initLights(){
    //set directional scene light values
    scenelight.pos = glm::vec3(1,0,0);
    scenelight.ambient = glm::vec3(0.025f,0.025f,0.1f);
    scenelight.diffuse = glm::vec3(0.8f,0.8f,0.25f);

    WorldPointLightObject pointLight;
    pointLight.pos = glm::vec3(4005,0,-5);
    pointLight.scale = glm::vec3(0.2f, 0.2f, 0.2f);
    pointLight.ambient = glm::vec3(0.9f,0.025f,0.1f);
    pointLight.diffuse = glm::vec3(0.9f,0.6f,0.5f);
    pointLight.specular = glm::vec3(0.8f,0.5f,0.5f);
    pointLight.attenuation = glm::vec3(1, 0.7f, 1.8f);
    pointLights.push_back(pointLight);

    //add a spot light object
    WorldSpotLightObject spotlight;
    spotlight.pos = {4000,0,-5};
    spotlight.scale = {0.05f,0.05f,0.05f};
    spotlight.diffuse = {0,1,1};
    spotlight.specular = {1,1,1};
    spotlight.attenuation = {1,0.07f,0.017f};
    spotlight.direction = glm::vec3(4010,50,-5) - spotlight.pos; //temp direction
    spotlight.cutoffs = {glm::cos(glm::radians(10.0f)), glm::cos(glm::radians(25.0f))};

    spotLights.push_back(spotlight);
}

//initialize bullet physics engine
void WorldState::initBullet(){
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

void WorldState::loadCollisionMeshes(VulkanEngine& engine){ 
    //load asteroid mesh and configure rigidbody and GImpactShape
    {
        //misleading naming, this method should be renamed to get_mesh_indices
        //get mesh indices for asteroid
        Mesh* asteroid = engine.get_mesh("asteroid");
        int base = asteroid->indexBase;
        int count = asteroid->indexCount;
        std::vector<Vertex>& allV = engine.get_allVertices(); //reference all loaded model vertices
        std::vector<uint32_t>& allI = engine.get_allIndices(); //reference all loaded model indices

        btTriangleMesh *mTriMesh = new btTriangleMesh(true, false);

        //create a trimesh by stepping through allVertices using allIndices to select the correct vertices
        for (int i = base; i < base+count; i+=3) {
            btVector3 v1(allV[allI[i]].pos.x, allV[allI[i]].pos.y, allV[allI[i]].pos.z);
            btVector3 v2(allV[allI[i+1]].pos.x, allV[allI[i+1]].pos.y, allV[allI[i+1]].pos.z);
            btVector3 v3(allV[allI[i+2]].pos.x, allV[allI[i+2]].pos.y, allV[allI[i+2]].pos.z);
            mTriMesh->addTriangle(v1, v2, v3, true);
        }
        btGImpactMeshShape *asteroidCollisionShape = new btGImpactMeshShape(mTriMesh);
        //asteroidCollisionShape->setMargin(0.05);
        asteroidCollisionShape->setLocalScaling(btVector3(objects[4].scale.x, objects[4].scale.y, objects[4].scale.z)); //may not be 1:1 scale between bullet and vulkan
        asteroidCollisionShape->updateBound();// Call this method once before doing collisions 
        collisionShapes.push_back(asteroidCollisionShape);

        btTransform asteroidTransform;
        asteroidTransform.setIdentity();
        asteroidTransform.setOrigin(btVector3(objects[4].pos.x, objects[4].pos.y, objects[4].pos.z));

        //set initial rotation of asteroid
        btQuaternion quat;
        if(RANDOMIZE_START)
            quat.setEulerZYX(getRandFloat(0,359),getRandFloat(0,359),getRandFloat(0,359));
        else
            quat.setEulerZYX(0,0,0);
        asteroidTransform.setRotation(quat);

        btScalar mass(100000);
        //rigidbody is dynamic if and only if mass is non zero, otherwise static
        bool isDynamic = (mass != 0.f);
        btVector3 localInertia(0, 0, 0);
        if (isDynamic)
            asteroidCollisionShape->calculateLocalInertia(mass, localInertia);

        //using motionstate is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
        btDefaultMotionState* myMotionState = new btDefaultMotionState(asteroidTransform);
        btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, asteroidCollisionShape, localInertia);
        rbInfo.m_friction = 5.0f;
        btRigidBody* asteroidRB = new btRigidBody(rbInfo);
        asteroidRB->setActivationState(DISABLE_DEACTIVATION); //stop body from disabling collision, bullet threshholds are pretty loose

        //set initial rotational velocity of asteroid
        if(RANDOMIZE_START){
            float f = MAX_ASTEROID_ROTATION_VELOCITY;
            asteroidRB->setAngularVelocity(btVector3(getRandFloat(-f,f),getRandFloat(-f,f),getRandFloat(-f,f))); //random rotation of asteroid
        }
        else
            asteroidRB->setAngularVelocity(btVector3(0.005f, 0.015f, 0.01f)); //initial rotation of asteroid
        dynamicsWorld->addRigidBody(asteroidRB);//add the body to the dynamics world
    }

    //load lander collision mesh and configure rigidbody and compound shape
    {
		//create a dynamic rigidbody
		btCollisionShape* landerShape = new btBoxShape(btVector3(1,1,1));
        //colShape->setMargin(0.05);
        landerShape->setLocalScaling(btVector3(objects[2].scale.x, objects[2].scale.y, objects[2].scale.z)); //may not be 1:1 scale between bullet and vulkan
		collisionShapes.push_back(landerShape);

		/// create lander transform
		btTransform landerTransform;
		landerTransform.setIdentity();
        if(RANDOMIZE_START){
            landerTransform.setOrigin(getPointOnSphere(getRandFloat(0,360), getRandFloat(0,360), LANDER_START_DISTANCE));
        }
        else
            landerTransform.setOrigin(btVector3(objects[2].pos.x, objects[2].pos.y, objects[2].pos.z));
		btScalar mass(10);
		//rigidbody is dynamic if and only if mass is non zero, otherwise static
		bool isDynamic = (mass != 0.f);
        btVector3 localInertia(0, 0, 0);
		if (isDynamic)
			landerShape->calculateLocalInertia(mass, localInertia);
		//using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
		btDefaultMotionState* myMotionState = new btDefaultMotionState(landerTransform);
		btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, landerShape, localInertia);
        rbInfo.m_friction = 2.0f;
        //rbInfo.m_spinningFriction  = 0.5f;
        //rbInfo.m_rollingFriction = 0.5f;
		btRigidBody* landerRB = new btRigidBody(rbInfo);
        landerRB->setActivationState(DISABLE_DEACTIVATION); //stop body from disabling collision, bullet threshholds are pretty loose

        //calculate initial direction of travel
        btVector3 direction;
        if(LANDER_COLLISION_COURSE || !RANDOMIZE_START)
            //dest-current position is the direction, dest is origin so just negate
            direction = -landerTransform.getOrigin();
        else
            //instead of origin pick a random point LANDER_START_DISTANCE away from the asteroid
            direction = getPointOnSphere(getRandFloat(0,360), getRandFloat(0,360), LANDER_PASS_DISTANCE)-landerTransform.getOrigin();

        landerRB->setLinearVelocity(direction.normalize()*INITIAL_LANDER_SPEED); //start box falling towards asteroid

		dynamicsWorld->addRigidBody(landerRB);
	}
}

//helper, gets random btvector between min and max
float WorldState::getRandFloat(float min, float max){
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
btVector3 WorldState::getPointOnSphere(float pitch, float yaw, float radius){
    btVector3 result;
    result.setX(radius * cos(glm::radians(yaw)) * sin(glm::radians(pitch)));
    result.setY(radius * sin(glm::radians(yaw)) * sin(glm::radians(pitch)));
    result.setZ(radius * cos(glm::radians(pitch)));
    return result;
}

void WorldState::changeSimSpeed(int pos, bool pause){
    if(pause){ //toggle pause on and off
        if(getWorldStats().timeStepMultiplier == 0)
            setSimSpeedMultiplier(SIM_SPEEDS[selectedSimSpeedIndex]);
        else
            setSimSpeedMultiplier(0);
    }
    else{
        if(pos == 0) //0 will be a normal speed shortcut eventually?
            selectedSimSpeedIndex = 2;
        else{
            selectedSimSpeedIndex+=pos;
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
WorldState::WorldState(){
    initLights();
    initBullet();
    updateDeltaTime();
}

WorldState::~WorldState(){
    cleanupBullet();
}

void WorldState::cleanupBullet(){
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