#pragma once
#include "obj.h"
//#include <btBulletDynamicsCommon.h>
//#include <BulletCollision/Gimpact/btGImpactShape.h>
//#include <BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h>

struct CollisionObj : virtual WorldObject {
    //btCollisionS collisionShape;
    bool concaveTriangleShape = false;
    bool boxShape = false;
    int mass;
   //virtual ~CollisionObj(){};
};