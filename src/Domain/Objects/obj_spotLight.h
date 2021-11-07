#pragma once
#include "obj_pointLight.h"
#include <memory>

struct WorldSpotLightObject : WorldPointLightObject{
    glm::vec3 direction; // 
    glm::vec2 cutoffs; // x is inner y is outer
    glm::vec2 cutoffAngles;
};