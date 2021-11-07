#pragma once
#include <bullet/btBulletDynamicsCommon.h>

struct LanderObj;
class Mediator;

class MyDynamicsWorld : public btDiscreteDynamicsWorld{
    using btDiscreteDynamicsWorld::btDiscreteDynamicsWorld; //C++11 way to pass through constructor with parameters

    private:
    LanderObj* p_lander;

    public:
    void init(Mediator& mediator);
    void applyGravity();
};