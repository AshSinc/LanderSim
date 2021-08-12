#pragma once
struct SceneData{
    bool RANDOMIZE_START = true;
    bool LANDER_COLLISION_COURSE = false;
    float LANDER_START_DISTANCE = 1000.0f;
    float LANDER_PASS_DISTANCE = 750.0f;
    float INITIAL_LANDER_SPEED = 1.0f;
    float ASTEROID_MAX_ROTATIONAL_VELOCITY = 0.025f;
    float ASTEROID_SCALE = 75.0f;
    float GRAVITATIONAL_FORCE_MULTIPLIER = ASTEROID_SCALE/100;
};

struct ScenarioData_Scenario1: SceneData{
    ScenarioData_Scenario1(){
        RANDOMIZE_START = true;
        LANDER_COLLISION_COURSE = false;
        LANDER_START_DISTANCE = 1000.0f;
        LANDER_PASS_DISTANCE = 750.0f;
        INITIAL_LANDER_SPEED = 2.0f;
        ASTEROID_MAX_ROTATIONAL_VELOCITY = 0.025f;
        ASTEROID_SCALE = 75.0f;
        GRAVITATIONAL_FORCE_MULTIPLIER = 0.5f;
    }
};

struct ScenarioData_Scenario2: SceneData{
    ScenarioData_Scenario2(){
        RANDOMIZE_START = true;
        LANDER_COLLISION_COURSE = false;
        LANDER_START_DISTANCE = 250.0f;
        LANDER_PASS_DISTANCE = 200.0f;
        INITIAL_LANDER_SPEED = 2.0f;
        ASTEROID_MAX_ROTATIONAL_VELOCITY = 0.025f;
        ASTEROID_SCALE = 50.0f;
        GRAVITATIONAL_FORCE_MULTIPLIER = 0.2f;
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
        ASTEROID_SCALE = 100.0f;
        GRAVITATIONAL_FORCE_MULTIPLIER = 2.0f;
    }
};