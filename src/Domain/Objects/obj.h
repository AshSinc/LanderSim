#pragma once

#define GLM_FORCE_RADIANS //makes sure GLM uses radians to avoid confusion
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES //forces GLM to use a version of vec2 and mat4 that have the correct alignment requirements for Vulkan
//#define GLM_FORCE_DEPTH_ZERO_TO_ONE //forces GLM to use depth range of 0 to 1, instead of -1 to 1 as in OpenGL
#include <glm/glm.hpp>
#include <iostream>

struct WorldObject{
    glm::vec3 pos;
    glm::vec3 scale{1,1,1};
    glm::mat4 rot = glm::mat4 {1.0f};
    uint32_t id;
    void printString(){
        std::cout << "ObjID: " << pos.x << "\n";
        std::cout << "pos: (" << pos.x << ", " << pos.y << ", " << pos.z << ")\n";
    };
    virtual ~WorldObject(){};
};