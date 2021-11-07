#pragma once

#define GLM_FORCE_RADIANS //makes sure GLM uses radians to avoid confusion
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES //forces GLM to use a version of vec2 and mat4 that have the correct alignment requirements for Vulkan
//#define GLM_FORCE_DEPTH_ZERO_TO_ONE //forces GLM to use depth range of 0 to 1, instead of -1 to 1 as in OpenGL
#include <glm/glm.hpp>
#include <iostream>
#include <glm/gtx/string_cast.hpp>
struct btVector3;

struct WorldObject{
    glm::vec3 pos;
    glm::vec3 initialPos; //used to update landing site boxes (need the initial position because the asteroid rotational matrix is based on the starting pos), should be moved
    float yaw = 0, pitch = 0, roll = 0; //used to update landing site and boxes when inputting rotation, should be moved to landingsite class/object
    glm::vec3 scale{1,1,1};
    glm::mat4 rot = glm::mat4{1.0f};
    glm::mat4 initialRot = glm::mat4{1.0f};
    glm::vec3 up;
    glm::vec3 forward;
    uint32_t id;
    //std::vector<WorldObject*> attachedObjects; //contains any objects that are attached, such as lights

    /*void attachObject(WorldObject* obj){
        attachedObjects.emplace_back(obj);
    }

    void updateAttachedObjects(){
        for(WorldObject* obj : attachedObjects){
            glm::vec3 rotated_point = rot * glm::vec4(obj->initialPos, 1);
            obj->pos = obj->initialPos + pos;
            obj->rot = rot*obj->initialRot; //rot isnt used for lights,
        }
        
    }*/

    void printString(){
        std::cout << "ObjID: " << id << "\n";
        std::cout << "pos: (" << pos.x << ", " << pos.y << ", " << pos.z << ")\n";
    };
    
    virtual ~WorldObject(){
        
    };
};