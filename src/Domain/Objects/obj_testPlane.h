#pragma once
#include "obj.h"
#include "obj_render.h"
#include "data_scene.h"
#include "dmn_myScene.h"
#include  <vector>
#include "mediator.h"
#include <iostream>
#include <glm/gtx/string_cast.hpp>
#include <glm/glm.hpp>

#include "sv_randoms.h"

struct TestPlaneObj : virtual WorldObject{

    const int RADIUS = 24;
    const int NUM_OBJS = 64;
    const float ANGLE_INCREMENT = 360/NUM_OBJS;
    const int CENTER_X = 0;
    const int CENTER_Y = 0;
    const int HEIGHT = 22;
    const float SCALE = 0.5f;
    const float RANDOM_VARIANCE = 10;

    const int GRID_OFFSET = -30;

    Mediator* p_mediator;

    TestPlaneObj(Mediator* mediator): p_mediator{mediator}{}

    void constructPlane(SceneData& sceneData, 
            std::vector<std::shared_ptr<WorldObject>>* objects,
            std::vector<std::shared_ptr<RenderObject>>* renderableObjects,
            MyScene* myScene
            ){

        float z = HEIGHT*sceneData.ASTEROID_SCALE;
        float scale = SCALE*sceneData.ASTEROID_SCALE;

        //construct a grid
        int i = 0;
        int gridWidth = std::sqrt(NUM_OBJS);
        for(int yi = 0; yi < gridWidth; yi++){
            for(int xi = 0; xi < gridWidth; xi++){
                
                float x = xi*10+GRID_OFFSET + Service::getRandFloat(-RANDOM_VARIANCE,RANDOM_VARIANCE);
                float y = yi*10+GRID_OFFSET + Service::getRandFloat(-RANDOM_VARIANCE,RANDOM_VARIANCE);

                std::shared_ptr<RenderObject> planePoint = std::shared_ptr<RenderObject>(new RenderObject());
                planePoint->id = id++;
                planePoint->pos = glm::vec3(x,y,z);
                planePoint->initialPos = glm::vec3(x,y,z);
                planePoint->scale = glm::vec3(scale,scale,scale);
                myScene->setRendererMeshVars("sphere" + std::to_string(i%10), planePoint.get());
                planePoint->material = p_mediator->renderer_getMaterial("unlitmesh"); //defaultmesh //unlitmesh
                planePoint->material->diffuse = glm::vec3(1,1,1);
                planePoint->material->specular = glm::vec3(1,1,1);
                planePoint->material->extra.x = 32;
                planePoint->altMaterial = p_mediator->renderer_getMaterial("greyscale_unlitmesh");
                planePoint->altMaterial->diffuse = glm::vec3(1,1,1);
                planePoint->altMaterial->specular = glm::vec3(1,1,1);
                planePoint->altMaterial->extra.x = 32;
                objects->push_back(planePoint);
                renderableObjects->push_back(planePoint);

                i++;
            }
        }

        //construct a circle
        /*float angle = 0;
        int radius = RADIUS*sceneData.ASTEROID_SCALE;
        int cx = -CENTER_X*sceneData.ASTEROID_SCALE;
        int cy = -CENTER_Y*sceneData.ASTEROID_SCALE;
        for(int i = 0; i < NUM_OBJS; i ++){



            float x = cx + radius * glm::cos(angle) + Service::getRandFloat(-RANDOM_VARIANCE,RANDOM_VARIANCE);
            float y = cy + radius * glm::sin(angle) + Service::getRandFloat(-RANDOM_VARIANCE,RANDOM_VARIANCE);

            angle+=glm::radians(ANGLE_INCREMENT);

            std::shared_ptr<RenderObject> planePoint = std::shared_ptr<RenderObject>(new RenderObject());
            planePoint->id = id++;
            planePoint->pos = glm::vec3(x,y,z);
            planePoint->initialPos = glm::vec3(x,y,z);
            planePoint->scale = glm::vec3(scale,scale,scale);
            myScene->setRendererMeshVars("sphere" + std::to_string(i%10), planePoint.get());
            planePoint->material = p_mediator->renderer_getMaterial("defaultmesh"); //unlitmesh
            planePoint->material->diffuse = glm::vec3(1,1,1);
            planePoint->material->specular = glm::vec3(1,1,1);
            planePoint->material->extra.x = 32;
            planePoint->altMaterial = p_mediator->renderer_getMaterial("greyscale_unlitmesh");
            planePoint->altMaterial->diffuse = glm::vec3(1,1,1);
            planePoint->altMaterial->specular = glm::vec3(1,1,1);
            planePoint->altMaterial->extra.x = 32;
            objects->push_back(planePoint);
            renderableObjects->push_back(planePoint);
        }*/

    }

    void updateTestPlaneObjects(){
        WorldObject& asteroidRenderObject = p_mediator->scene_getFocusableObject("Asteroid");
        
        for(int i = 0; i < NUM_OBJS; i++){
            WorldObject& planePoint = p_mediator->scene_getWorldObject(11+i); //11 is the first index of plane marker obj in the world object list ISSUE - need an object mgr
            glm::vec3 rotated_point = initialRot*asteroidRenderObject.rot * glm::vec4(planePoint.initialPos, 1); //*initialRot to subtract asteroid rotation at time of reset, see below. 
            planePoint.pos = rotated_point;
        }
    }

    void resetTestPlaneObjects(){
        WorldObject& asteroidRenderObject = p_mediator->scene_getFocusableObject("Asteroid");
        initialRot = glm::inverse(asteroidRenderObject.rot); //simple hack, we store the inverse of asteroids current rotation so we can subtract it with matrix multiplication above, necessary because we are bound to asteroid rotation in update function
        
        std::cout << "here \n"; 
        for(int i = 0; i < NUM_OBJS; i++){
            WorldObject& planePoint = p_mediator->scene_getWorldObject(11+i); //11 is the first index of plane marker obj in the world object list ISSUE - need an object mgr
            planePoint.pos = planePoint.initialPos;
        }
    }
};