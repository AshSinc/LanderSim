#pragma once
#include "obj.h"

struct WorldLightObject : virtual WorldObject{
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular{1,1,1};
    ~WorldLightObject(){};
};