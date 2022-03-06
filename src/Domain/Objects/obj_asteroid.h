#pragma once
#include "obj_collisionRender.h"
#include <glm/gtx/fast_square_root.hpp>
#include "sv_randoms.h"
#include "mediator.h"
#include <BulletCollision/Gimpact/btGImpactShape.h>
#include <BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h>
#include <glm/gtx/string_cast.hpp>

struct AsteroidObj : virtual CollisionRenderObj{
    float maxRotationVelocity = 0.025f;
    bool randomStartRotation = false;
    btQuaternion initialRotation;
    btVector3 angularVelocity;

    void init(btAlignedObjectArray<btCollisionShape*>* collisionShapes, btDiscreteDynamicsWorld* dynamicsWorld, Mediator& r_mediator){
        std::vector<Vertex>& allV = r_mediator.renderer_getAllVertices(); //reference all loaded model vertices
        std::vector<uint32_t>& allI = r_mediator.renderer_getAllIndices(); //reference all loaded model indices
    
        btGImpactMeshShape* collisionShape;

        if(!collisionShape){
            btTriangleMesh *mTriMesh = new btTriangleMesh(true, false);

            //create a trimesh by stepping through allVertices using allIndices to select the correct vertices
            for (int i = indexBase; i < indexBase+indexCount; i+=3) {
                btVector3 v1(allV[allI[i]].pos.x, allV[allI[i]].pos.y, allV[allI[i]].pos.z);
                btVector3 v2(allV[allI[i+1]].pos.x, allV[allI[i+1]].pos.y, allV[allI[i+1]].pos.z);
                btVector3 v3(allV[allI[i+2]].pos.x, allV[allI[i+2]].pos.y, allV[allI[i+2]].pos.z);
                mTriMesh->addTriangle(v1, v2, v3, true);
            }
            collisionShape = new btGImpactMeshShape(mTriMesh);
        }

        //asteroidCollisionShape->setMargin(0.05);
        collisionShape->setLocalScaling(btVector3(scale.x, scale.y, scale.z));
        collisionShape->updateBound();// Call this method once before doing collisions 
        collisionShapes->push_back(collisionShape);

        btTransform transform;
        transform.setIdentity();

        initTransform(&transform);

        btScalar btMass(mass);
        //rigidbody is dynamic if and only if mass is non zero, otherwise static
        bool isDynamic = (btMass != 0.f);
        btVector3 localInertia(0, 0, 0);
        if (isDynamic)
            collisionShape->calculateLocalInertia(btMass, localInertia);

        //using motionstate is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
        btDefaultMotionState* myMotionState = new btDefaultMotionState(transform);
        btRigidBody::btRigidBodyConstructionInfo rbInfo(btMass, myMotionState, collisionShape, localInertia);
        rbInfo.m_friction = 1.0f; //5 is sticky
        btRigidBody* rigidbody = new btRigidBody(rbInfo);
        rigidbody->setActivationState(DISABLE_DEACTIVATION); //stop body from disabling collision, bullet threshholds are pretty loose

        initRigidBody(&transform, rigidbody);

        dynamicsWorld->addRigidBody(rigidbody);//add the body to the dynamics world
    }

    void timestepBehaviour(btRigidBody* body, float timeStep){
        //we are clearing forces on timestep because while lander is in contact bullet will sometimes apply a small force on the asteroid, despite mass difference
        body->clearForces();
        body->setLinearVelocity(btVector3(0,0,0));
        //angularVelocity = Service::bt2glm(body->getAngularVelocity());
    }

    void initTransform(btTransform* transform){
        transform->setOrigin(btVector3(pos.x, pos.y, pos.z)); //astoid always at zero
        //btQuaternion quat;
        //if(randomStartRotation)
        //    quat.setEulerZYX(Service::getRandFloat(0,359),Service::getRandFloat(0,359),Service::getRandFloat(0,359));
        //else
        //    quat.setEulerZYX(0,0,0);
        transform->setRotation(initialRotation);
    }
    
    void initRigidBody(btTransform* transform, btRigidBody* rigidbody){
        rigidbody->setAngularVelocity(angularVelocity); //random rotation of asteroid
    }

    void applyImpulse(btRigidBody* rigidbody, btVector3 vector, float duration){};

    ~AsteroidObj(){};
};