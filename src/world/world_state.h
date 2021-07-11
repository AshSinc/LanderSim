#pragma once
#define GLM_FORCE_RADIANS //makes sure GLM uses radians to avoid confusion
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES //forces GLM to use a version of vec2 and mat4 that have the correct alignment requirements for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE //forces GLM to use depth range of 0 to 1, instead of -1 to 1 as in OpenGL
#include <glm/glm.hpp>
#include <vector>
#include <chrono>
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/Gimpact/btGImpactShape.h>
#include <BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h>

class VulkanEngine; //forward reference, because we reference this before defining it
class Mesh; //forward reference, because we reference this before defining it

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

struct WorldCamera{
    glm::vec3 cameraPos = glm::vec3(2.0f, 2.0f, 2.0f);
    glm::vec3 cameraFront = glm::vec3(-2.0f, -2.0f, -2.0f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 0.0f, 1.0f);
};
//class VulkanEngine;
class WorldState{
    private:
    static WorldCamera camera;
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
    static bool RANDOMIZE_START;
    static bool LANDER_COLLISION_COURSE;
    static float MAX_ASTEROID_ROTATION_VELOCITY;
    static float LANDER_START_DISTANCE;
    static float LANDER_PASS_DISTANCE;
    static float INITIAL_LANDER_SPEED;
    static float ASTEROID_GRAVITATIONAL_FORCE;
    float getRandFloat(float min, float max);
    btVector3 getPointOnSphere(float pitch, float yaw, float radius);


public:
    void loadCollisionMeshes(VulkanEngine& engine); //load bullet collision meshes, 
    static float deltaTime; // Time between current frame and last frame
    std::chrono::_V2::system_clock::time_point lastTime{}; // Time of last frame
    void updateDeltaTime();
    void worldTick();
    static WorldCamera& getWorldCamera();
    static std::vector<WorldObject>& getWorldObjects();
    std::vector<WorldPointLightObject>& getWorldPointLightObjects();
    std::vector<WorldSpotLightObject>& getWorldSpotLightObjects();
    
    WorldLightObject scenelight;
    std::vector<WorldPointLightObject> pointLights;
    std::vector<WorldSpotLightObject> spotLights;

    static std::vector<WorldObject> objects;

    void addObject(std::vector<WorldObject>& container, WorldObject obj);

    void mainLoop();
    WorldState();
    ~WorldState();
};

