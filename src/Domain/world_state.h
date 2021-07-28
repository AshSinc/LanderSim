#pragma once
#define GLM_FORCE_RADIANS //makes sure GLM uses radians to avoid confusion
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES //forces GLM to use a version of vec2 and mat4 that have the correct alignment requirements for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE //forces GLM to use depth range of 0 to 1, instead of -1 to 1 as in OpenGL
#include <glm/glm.hpp>
#include <vector>
#include <chrono>
#include <btBulletDynamicsCommon.h>
//#include <btBulletDynamicsCommon.h>
#include <BulletCollision/Gimpact/btGImpactShape.h>
#include <BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h>

namespace Vk{
    class Renderer; //forward reference, because we reference this before defining it
}

class Mesh; //forward reference, because we reference this before defining it
class WorldInput;

//these should be moved out of here
struct WorldObject{
    glm::vec3 pos;
    glm::vec3 scale{1,1,1};
    glm::mat4 rot = glm::mat4 {1.0f};
    glm::vec3 velocity{0,0,0};
    glm::vec3 acceleration{0,0,0}; //will likely be a vector to a gravitational body
};

struct WorldLightObject : WorldObject{
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular{1,1,1};
};

struct WorldPointLightObject : WorldLightObject{
    glm::vec3 attenuation; //x constant, y linear, z quadratic
};

struct WorldSpotLightObject : WorldPointLightObject{
    glm::vec3 direction; // 
    glm::vec2 cutoffs; // x is inner y is outer
};
//these should be moved out of here

class WorldState{
public:
    struct WorldStats{
        float timeStepMultiplier = 1;
        float gravitationalForce = 0;
        float landerVelocity = 0;
        float lastImpactForce = 0;
        float largestImpactForce = 0;
    }worldStats;
    WorldStats getWorldStats();
    void setSimSpeedMultiplier(float multiplier);
    
    void loadCollisionMeshes(Vk::Renderer& engine); //load bullet collision meshes, 
    static double deltaTime; // Time between current frame and last frame
    std::chrono::_V2::system_clock::time_point lastTime{}; // Time of last frame
    void updateDeltaTime();
    void worldTick();
    WorldState* getWorld();
    std::vector<WorldObject>& getWorldObjectsRef();
    std::vector<WorldPointLightObject>& getWorldPointLightObjects();
    std::vector<WorldSpotLightObject>& getWorldSpotLightObjects();

    void updateCamera(glm::vec3 newPos, glm::vec3 front);
    
    WorldLightObject scenelight;
    std::vector<WorldPointLightObject> pointLights;
    std::vector<WorldSpotLightObject> spotLights;

    std::vector<WorldObject> objects{}; //init default
    void addObject(std::vector<WorldObject>& container, WorldObject obj);

    void mainLoop();
    WorldState();
    ~WorldState();

    void changeSimSpeed(int pos, bool pause);

private:
    //Bullet vars
    btDiscreteDynamicsWorld* dynamicsWorld;
    btBroadphaseInterface* overlappingPairCache;
    ///the default constraint solver. For parallel processing you can use a different solver (see Extras/BulletMultiThreaded)
    btSequentialImpulseConstraintSolver* solver;
    btCollisionDispatcher* dispatcher;
    btDefaultCollisionConfiguration* collisionConfiguration;
    //keep track of the shapes, we release memory at exit.
	//make sure to re-use collision shapes among rigid bodies whenever possible!
	btAlignedObjectArray<btCollisionShape*> collisionShapes;
    void initLights();
    void initBullet();
    void cleanupBullet();   
    bool RANDOMIZE_START = false;
    bool LANDER_COLLISION_COURSE = false;
    float MAX_ASTEROID_ROTATION_VELOCITY = 0.03f;
    float LANDER_START_DISTANCE = 150.0f;
    float LANDER_PASS_DISTANCE = 30.0f;
    float INITIAL_LANDER_SPEED = 1.5f;
    float ASTEROID_GRAVITATIONAL_FORCE = 0.5f;
    //physics sim vars
    int SUBSTEP_SAFETY_MARGIN = 1; //need to redo timestep code completely

    float getRandFloat(float min, float max);
    btVector3 getPointOnSphere(float pitch, float yaw, float radius);
    int selectedSimSpeedIndex = 2;
    float SIM_SPEEDS[7] {0.25f,0.5f,1,2,4,8,16};
};

