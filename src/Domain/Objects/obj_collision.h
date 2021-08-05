#pragma once
#include "obj.h"

struct CollisionObj : virtual WorldObject {
    int mass;
    virtual ~CollisionObj(){};
};