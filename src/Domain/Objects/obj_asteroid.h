#pragma once
#include "obj_collisionRender.h"
#include <glm/gtx/fast_square_root.hpp>
#include "sv_randoms.h"
#include "mediator.h"
#include <BulletCollision/Gimpact/btGImpactShape.h>
#include <BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h>

//#include "../Service/BulletExtras/BulletWorldImporter/btBulletWorldImporter.h"

struct AsteroidObj : virtual CollisionRenderObj{
    float maxRotationVelocity = 0.025f;
    bool randomStartRotation = false;

    void init(btAlignedObjectArray<btCollisionShape*>* collisionShapes, btDiscreteDynamicsWorld* dynamicsWorld, Mediator& r_mediator){
        std::vector<Vertex>& allV = r_mediator.renderer_getAllVertices(); //reference all loaded model vertices
        std::vector<uint32_t>& allI = r_mediator.renderer_getAllIndices(); //reference all loaded model indices
    
        btGImpactMeshShape* collisionShape;

        //ISSUE
        //Attempting to read collisionshape from .bullet file
        //fails as numShape always == 0
        //SOLUTION
        //Either accept longer loading times, find a fix, or find another way
        try{
            collisionShape = readBtShape();
        }
        catch(std::invalid_argument e){
            std::cout << e.what() << "\n";
            collisionShape = NULL;
        }
        if(!collisionShape){
            btTriangleMesh *mTriMesh = new btTriangleMesh(true, false);

            //create a trimesh by stepping through allVertices using allIndices to select the correct vertices
            for (int i = indexBase; i < indexBase+indexCount; i+=3) {
                btVector3 v1(allV[allI[i]].pos.x, allV[allI[i]].pos.y, allV[allI[i]].pos.z);
                btVector3 v2(allV[allI[i+1]].pos.x, allV[allI[i+1]].pos.y, allV[allI[i+1]].pos.z);
                btVector3 v3(allV[allI[i+2]].pos.x, allV[allI[i+2]].pos.y, allV[allI[i+2]].pos.z);
                mTriMesh->addTriangle(v1, v2, v3, true);
                //mTriMesh->
                //btTriangleIndexVertexArray 
            }
            
            //btTriangleIndexVertexArray m_indexVertexArrays = btTriangleIndexVertexArray(numTris,&triIndex[0][0],3*sizeof(int), numVerts, allV, sizeof(allV));
            collisionShape = new btGImpactMeshShape(mTriMesh);
            //collisionShape->getMeshInterface();

            //serialiseBtShape(collisionShape); // <-- doesnt work anyway
        }

        /*btTriangleIndexVertexArray* m_indexVertexArrays = new btTriangleIndexVertexArray
		(numTris,
		&triIndex[0][0],
		3*sizeof(int),
		numVerts,
		(btScalar*) vertValues,
		sizeof(btScalar)*3);*/

        /*btTriangleMesh *mTriMesh = new btTriangleMesh(true, false);

        //create a trimesh by stepping through allVertices using allIndices to select the correct vertices
        for (int i = indexBase; i < indexBase+indexCount; i+=3) {
            btVector3 v1(allV[allI[i]].pos.x, allV[allI[i]].pos.y, allV[allI[i]].pos.z);
            btVector3 v2(allV[allI[i+1]].pos.x, allV[allI[i+1]].pos.y, allV[allI[i+1]].pos.z);
            btVector3 v3(allV[allI[i+2]].pos.x, allV[allI[i+2]].pos.y, allV[allI[i+2]].pos.z);
            mTriMesh->addTriangle(v1, v2, v3, true);
        }
        btGImpactMeshShape *collisionShape = new btGImpactMeshShape(mTriMesh);*/
        //asteroidCollisionShape->setMargin(0.05);
        collisionShape->setLocalScaling(btVector3(scale.x, scale.y, scale.z)); //may not be 1:1 scale between bullet and vulkan
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
        rbInfo.m_friction = 10.0f;
        btRigidBody* rigidbody = new btRigidBody(rbInfo);
        rigidbody->setActivationState(DISABLE_DEACTIVATION); //stop body from disabling collision, bullet threshholds are pretty loose

        initRigidBody(&transform, rigidbody);

        dynamicsWorld->addRigidBody(rigidbody);//add the body to the dynamics world
    }

    /*void serialiseBtShape(btGImpactMeshShape* meshShape){
        int maxSerializeBufferSize = 1024*1024*5;
        btDefaultSerializer* serializer = new btDefaultSerializer(maxSerializeBufferSize);
        serializer->setSerializationFlags(BT_SERIALIZE_NO_BVH);
        serializer->startSerialization();
        meshShape->serializeSingleShape(serializer);
        serializer->finishSerialization();
        FILE* f2 = fopen("asteroidshape.bullet","wb");
        fwrite(serializer->getBufferPointer(),serializer->getCurrentBufferSize(),1,f2);
        fclose(f2);
    }*/

    btGImpactMeshShape* readBtShape(){
        /*btBulletWorldImporter import(0);//don't store info into the world
        if (import.loadFile("asteroidshape.bullet")){
            int numShape = import.getNumCollisionShapes();
            if (numShape)
                return (btGImpactMeshShape*)import.getCollisionShapeByIndex(0);
            else
                throw std::invalid_argument("Invalid Bullet Collision Shape");
        }
        else*/
            throw std::invalid_argument( "Can't Find bullet asteroid shape file" );
    }

    /*btGImpactMeshShape* readBtShape(){
        btBulletWorldImporter import(0);//don't store info into the world
        if (import.loadFile("asteroidshape.bullet")){
            int numShape = import.getNumCollisionShapes();
            if (numShape)
                return (btGImpactMeshShape*)import.getCollisionShapeByIndex(0);
            else
                throw std::invalid_argument("Invalid Bullet Collision Shape");
        }
        else
            throw std::invalid_argument( "Can't Find bullet asteroid shape file" );
    }*/

    void initTransform(btTransform* transform){
        transform->setOrigin(btVector3(pos.x, pos.y, pos.z)); //astoid always at zero
        btQuaternion quat;
        if(randomStartRotation)
            quat.setEulerZYX(Service::getRandFloat(0,359),Service::getRandFloat(0,359),Service::getRandFloat(0,359));
        else
            quat.setEulerZYX(0,0,0);
        transform->setRotation(quat);
    }
    
    void initRigidBody(btTransform* transform, btRigidBody* rigidbody){
        //set initial rotational velocity of asteroid
        if(randomStartRotation){
            float f = maxRotationVelocity;
            rigidbody->setAngularVelocity(btVector3(Service::getRandFloat(-f,f),Service::getRandFloat(-f,f),Service::getRandFloat(-f,f))); //random rotation of asteroid
        }
        else
            rigidbody->setAngularVelocity(btVector3(0.005f, 0.015f, 0.01f)); //initial rotation of asteroid
    }

    ~AsteroidObj(){};
};