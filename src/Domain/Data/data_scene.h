#pragma once
#define GLM_FORCE_RADIANS //makes sure GLM uses radians to avoid confusion
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES //forces GLM to use a version of vec2 and mat4 that have the correct alignment requirements for Vulkan
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp> 

struct LandingSiteData{
    glm::vec3 pos = glm::vec3(0,0,50);
    float yaw = 0, pitch = 0, roll = 0;
    glm::mat4 rot = glm::yawPitchRoll(glm::radians(yaw), glm::radians(pitch), glm::radians(roll));
};

struct LandingSiteData_1: LandingSiteData{
    LandingSiteData_1(){
        pos = glm::vec3(-5.657385, -1.789196, 29.761101);
        yaw = 0, pitch = -3, roll = -10;
        rot = glm::yawPitchRoll(glm::radians(yaw), glm::radians(pitch), glm::radians(roll));
    }
};

struct LandingSiteData_2: LandingSiteData{
    LandingSiteData_2(){
        pos = glm::vec3(-43.423363, -6.744720, 11.310112);
        yaw = -73, pitch = 19, roll = -7;
        rot = glm::yawPitchRoll(glm::radians(yaw), glm::radians(pitch), glm::radians(roll));
    }
};

struct SceneData{
    bool RANDOMIZE_ROTATION = false;
    float LANDER_START_DISTANCE = 1000.0f;
    float ASTEROID_ROTATION_X = 0;
    float ASTEROID_ROTATION_Y = 0;
    float ASTEROID_ROTATION_Z = 0;
    int ASTEROID_SCALE = 2;
    float ASTEROID_MIN_ROTATIONAL_VELOCITY = 0.001f;
    float ASTEROID_MAX_ROTATIONAL_VELOCITY = 0.005f;
    float GRAVITATIONAL_FORCE_MULTIPLIER = ASTEROID_SCALE/30.0f;
    LandingSiteData landingSite = LandingSiteData_1();
    bool USE_ONLY_ESTIMATE = false;
};

struct ScenarioData_Scenario1: SceneData{
    ScenarioData_Scenario1(){
        RANDOMIZE_ROTATION = false;
        LANDER_START_DISTANCE = 1000.0f;
        ASTEROID_ROTATION_X = 0;
        ASTEROID_ROTATION_Y = 0;
        ASTEROID_ROTATION_Z = 0;
        ASTEROID_SCALE = 2;
        ASTEROID_MIN_ROTATIONAL_VELOCITY = 0.001f;
        ASTEROID_MAX_ROTATIONAL_VELOCITY = 0.008f;
        GRAVITATIONAL_FORCE_MULTIPLIER = ASTEROID_SCALE/30.0f;
        LandingSiteData landingSite = LandingSiteData_1();
    }
};

struct ScenarioData_Scenario2: SceneData{
    ScenarioData_Scenario2(){
        RANDOMIZE_ROTATION = true;
        LANDER_START_DISTANCE = 300.0f;
        ASTEROID_ROTATION_X = 0;
        ASTEROID_ROTATION_Y = 0;
        ASTEROID_ROTATION_Z = 0;
        ASTEROID_SCALE = 5;
        ASTEROID_MIN_ROTATIONAL_VELOCITY = 0.001f;
        ASTEROID_MAX_ROTATIONAL_VELOCITY = 0.008f;
        GRAVITATIONAL_FORCE_MULTIPLIER = ASTEROID_SCALE/15.0f;
        LandingSiteData landingSite = LandingSiteData_1();
    }
};

struct ScenarioData_Scenario3: SceneData{
    ScenarioData_Scenario3(){
        RANDOMIZE_ROTATION = true;
        LANDER_START_DISTANCE = 1000.0f;
        ASTEROID_ROTATION_X = 0;
        ASTEROID_ROTATION_Y = 0;
        ASTEROID_ROTATION_Z = 0;
        ASTEROID_SCALE = 10;
        ASTEROID_MIN_ROTATIONAL_VELOCITY = 0.001f;
        ASTEROID_MAX_ROTATIONAL_VELOCITY = 0.008f;
        GRAVITATIONAL_FORCE_MULTIPLIER = ASTEROID_SCALE/15.0f;
        LandingSiteData landingSite = LandingSiteData_1();
    }
};