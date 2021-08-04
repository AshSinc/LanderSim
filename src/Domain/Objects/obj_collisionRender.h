#pragma once
#include "obj_render.h"
#include "obj_collision.h"

struct CollisionRenderObj : virtual RenderObject, virtual CollisionObj{
    //btCollisionS collisionShape;
    //bool concaveTriangleShape = false;
    //bool boxShape = false;
    //int mass;
    //~CollisionRenderObj(){};
};