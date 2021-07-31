#pragma once
#include "obj_light.h"

struct WorldPointLightObject : WorldLightObject{
    glm::vec3 attenuation; //x constant, y linear, z quadratic
};