#pragma once
#define GLM_FORCE_RADIANS //makes sure GLM uses radians to avoid confusion
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES //forces GLM to use a version of vec2 and mat4 that have the correct alignment requirements for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE //forces GLM to use depth range of 0 to 1, instead of -1 to 1 as in OpenGL
#include <glm/glm.hpp>
#include <vector>
#include <chrono>
#include <bullet/btBulletDynamicsCommon.h>
#include "obj_collisionRender.h"
#include <map>
#include <memory>
#include "world_stats.h"
#include <deque>
#include <mutex>
#include <limits> //get float max value for infinite raycast default

namespace Vk{
    class Renderer; //forward reference, because we reference this before defining it
}

class Mesh; //forward reference, because we reference this before defining it
class WorldInput;
class Mediator;

class WorldPhysics{
public:
    WorldStats worldStats;

    WorldStats& getWorldStats();
    void setSimSpeedMultiplier(float multiplier);
    void loadCollisionMeshes(std::vector<std::shared_ptr<CollisionRenderObj>>* collisionObjects); //load bullet collision meshes, 
    
    double deltaTime = 0.0; // Time between current frame and last frame
    std::chrono::_V2::system_clock::time_point lastTime{}; // Time of last frame
    void updateDeltaTime();
    void worldTick();

    int getWorldObjectsCount();

    void updateCamera(glm::vec3 newPos, glm::vec3 front);
    
    void mainLoop();
    WorldPhysics(Mediator& mediator);
    ~WorldPhysics();

    void changeSimSpeed(int direction, bool pause);
    void reset();

    //void initDynamicsWorld();
    static void stepPreTickCallback(btDynamicsWorld *world, btScalar timeStep);
    static void stepPostTickCallback(btDynamicsWorld *world, btScalar timeStep);
    glm::vec3 performRayCast(glm::vec3 from, glm::vec3 dir, float range = std::numeric_limits<float>::max());

    double getTimeStamp(){return systemTimeStamp;};

private:
   
    double systemTimeStamp = 0; //tracked for file writing purposes
    int selectedSimSpeedIndex = 2;
    float SIM_SPEEDS[9] {0.25f,0.5f,1,2,4,8,16,32,64};
    int SPEED_ARRAY_SIZE = *(&SIM_SPEEDS + 1) - SIM_SPEEDS - 1; //get length of array (-1 because we want the last element) (https://www.educative.io/edpresso/how-to-find-the-length-of-an-array-in-cpp)
    
    //Bullet vars
    Mediator& r_mediator;
    btDiscreteDynamicsWorld* p_dynamicsWorld;
    //MyDynamicsWorld* p_dynamicsWorld;
    btBroadphaseInterface* overlappingPairCache;
    ///the default constraint solver. For parallel processing you can use a different solver (see Extras/BulletMultiThreaded)
    btSequentialImpulseConstraintSolver* solver;
    btCollisionDispatcher* dispatcher;
    btDefaultCollisionConfiguration* collisionConfiguration;
    //keep track of the shapes, we release memory at exit.
	//make sure to re-use collision shapes among rigid bodies whenever possible!
	btAlignedObjectArray<btCollisionShape*> collisionShapes;

    std::vector<std::shared_ptr<CollisionRenderObj>>* p_collisionObjects;

    void initLights();
    void initBullet();
    void cleanupBullet();   

    int SUBSTEP_SAFETY_MARGIN = 1; //need to redo timestep code completely

    void updateCollisionObjects(float timeStep);
    void checkCollisions();

    glm::mat4 rotateAround(glm::vec3 aPointToRotate, glm::vec3 aRotationCenter, glm::mat4 aRotationMatrix );
};

