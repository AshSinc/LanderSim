#pragma once
#include "obj_render.h"
#include "obj_collision.h"
#include "world_stats.h"
#include <bullet/btBulletDynamicsCommon.h>

class Mediator; //this should be temporary, mediator only needed for inneficient mesh loading of asteroid, this can be cleaned and half loading time, then mediator can be removed from function call

struct CollisionRenderObj : virtual RenderObject, virtual CollisionObj{
    btCollisionObject* p_btCollisionObject;
    virtual void applyImpulse(btRigidBody* rigidbody, btVector3 vector, float duration){};
    virtual void timestepBehaviour(btRigidBody* body, float timeStep = 0){};
    virtual void updateWorldStats(WorldStats* worldStats){}; //a little wierd to include this for only 1 object, but best solution for now to remove code from world_physics
    virtual void init(btAlignedObjectArray<btCollisionShape*>* collisionShapes, btDiscreteDynamicsWorld* dynamicsWorld, Mediator& r_mediator){};
    virtual ~CollisionRenderObj(){};
};