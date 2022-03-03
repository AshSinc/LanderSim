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

struct TestPlaneObj : virtual WorldObject{

    const int RADIUS = 24;
    const int NUM_OBJS = 10;
    const float ANGLE_INCREMENT = 360/NUM_OBJS;
    const int CENTER_X = -5;
    const int CENTER_Y = -5;
    const int HEIGHT = 22;
    const float SCALE = 1.0f;

    Mediator* p_mediator;

    TestPlaneObj(Mediator* mediator): p_mediator{mediator}{}

    void constructPlane(SceneData& sceneData, 
            std::vector<std::shared_ptr<WorldObject>>* objects,
            std::vector<std::shared_ptr<RenderObject>>* renderableObjects,
            MyScene* myScene
            ){
        

        //construct a grid

        //construct a circle
        float angle = 0;
        int radius = RADIUS*sceneData.ASTEROID_SCALE;
        int cx = -CENTER_X;
        int cy = -CENTER_Y;
        float z = HEIGHT*sceneData.ASTEROID_SCALE;
        float scale = SCALE*sceneData.ASTEROID_SCALE;
        for(int i = 0; i < NUM_OBJS; i ++){

            float x = cx + radius * glm::cos(angle);
            float y = cy + radius * glm::sin(angle);

            angle+=glm::radians(ANGLE_INCREMENT);

            std::shared_ptr<RenderObject> planePoint = std::shared_ptr<RenderObject>(new RenderObject());
            planePoint->id = id++;
            planePoint->pos = glm::vec3(x,y,z);
            planePoint->initialPos = planePoint->pos;
            planePoint->scale = glm::vec3(scale,scale,scale);
            myScene->setRendererMeshVars("sphere" + std::to_string(i+1), planePoint.get());
            planePoint->material = p_mediator->renderer_getMaterial("unlitmesh"); //unlitmesh
            planePoint->material->diffuse = glm::vec3(1,1,1);
            planePoint->material->specular = glm::vec3(1,1,1);
            planePoint->material->extra.x = 32;
            planePoint->altMaterial = p_mediator->renderer_getMaterial("greyscale_unlitmesh");
            planePoint->altMaterial->diffuse = glm::vec3(1,1,1);
            planePoint->altMaterial->specular = glm::vec3(1,1,1);
            planePoint->altMaterial->extra.x = 32;
            objects->push_back(planePoint);
            renderableObjects->push_back(planePoint);
        }

    }

    void updateTestPlaneObjects(){
        WorldObject& asteroidRenderObject = p_mediator->scene_getFocusableObject("Asteroid");
        glm::vec3 rotated_point = asteroidRenderObject.rot * glm::vec4(initialPos, 1);
        pos = rotated_point;
        
        for(int i = 0; i < NUM_OBJS; i++){
            WorldObject& sphere = p_mediator->scene_getWorldObject(11+i); //11 is the first index of plane marker obj in the world object list ISSUE - need an object mgr
            glm::vec3 rotated_point = asteroidRenderObject.rot * glm::vec4(sphere.initialPos, 1);
            sphere.pos = rotated_point;
        }
    }
};