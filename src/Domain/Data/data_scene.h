#pragma once
#define GLM_FORCE_RADIANS //makes sure GLM uses radians to avoid confusion
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES //forces GLM to use a version of vec2 and mat4 that have the correct alignment requirements for Vulkan
//#define GLM_FORCE_DEPTH_ZERO_TO_ONE //forces GLM to use depth range of 0 to 1, instead of -1 to 1 as in OpenGL
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp> 

struct LandingSiteData{
    glm::vec3 pos = glm::vec3(0,0,50);
    float yaw = 0, pitch = 0, roll = 0;
    glm::mat4 rot = glm::yawPitchRoll(glm::radians(yaw), glm::radians(pitch), glm::radians(roll));
    //glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
};

struct LandingSiteData_1: LandingSiteData{
    LandingSiteData_1(){
        pos = glm::vec3(-5.657385, -1.789196, 29.761101);
        yaw = 0, pitch = -6, roll = -11;
        rot = glm::yawPitchRoll(glm::radians(yaw), glm::radians(pitch), glm::radians(roll));
    }
};

struct LandingSiteData_2: LandingSiteData{
    LandingSiteData_2(){
        pos = glm::vec3(0,0,-50);
        yaw = 0, pitch = 0, roll = 0;
        rot = glm::yawPitchRoll(glm::radians(yaw), glm::radians(pitch), glm::radians(roll));
    }
};

struct SceneData{
    bool RANDOMIZE_START = false;
    bool LANDER_COLLISION_COURSE = false;
    float LANDER_START_DISTANCE = 1000.0f;
    float LANDER_PASS_DISTANCE = 750.0f;
    float INITIAL_LANDER_SPEED = 1.0f;
    float ASTEROID_MAX_ROTATIONAL_VELOCITY = 0.025f;
    //float ASTEROID_SCALE = 75.0f;
    int ASTEROID_SCALE = 1;
    float GRAVITATIONAL_FORCE_MULTIPLIER = ASTEROID_SCALE/100;
    LandingSiteData landingSite = LandingSiteData_1();
};

struct ScenarioData_Scenario1: SceneData{
    ScenarioData_Scenario1(){
        RANDOMIZE_START = false;
        LANDER_COLLISION_COURSE = false;
        LANDER_START_DISTANCE = 1000.0f;
        LANDER_PASS_DISTANCE = 750.0f;
        INITIAL_LANDER_SPEED = 2.0f;
        ASTEROID_MAX_ROTATIONAL_VELOCITY = 0.025f;
        ASTEROID_SCALE = 2;
        GRAVITATIONAL_FORCE_MULTIPLIER = 1.5f;
        LandingSiteData landingSite = LandingSiteData_1();
    }
};

struct ScenarioData_Scenario2: SceneData{
    ScenarioData_Scenario2(){
        RANDOMIZE_START = true;
        LANDER_COLLISION_COURSE = false;
        LANDER_START_DISTANCE = 300.0f;
        LANDER_PASS_DISTANCE = 200.0f;
        INITIAL_LANDER_SPEED = 1.5f;
        ASTEROID_MAX_ROTATIONAL_VELOCITY = 0.025f;
        ASTEROID_SCALE = 5;
        GRAVITATIONAL_FORCE_MULTIPLIER = 2.0f;
        LandingSiteData landingSite = LandingSiteData_1();
    }
};

struct ScenarioData_Scenario3: SceneData{
    ScenarioData_Scenario3(){
        RANDOMIZE_START = true;
        LANDER_COLLISION_COURSE = false;
        LANDER_START_DISTANCE = 1000.0f;
        LANDER_PASS_DISTANCE = 500.0f;
        INITIAL_LANDER_SPEED = 2.0f;
        ASTEROID_MAX_ROTATIONAL_VELOCITY = 0.025f;
        ASTEROID_SCALE = 10;
        GRAVITATIONAL_FORCE_MULTIPLIER = 5.0f;
        
    }
};