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

struct LandingSiteObj : virtual WorldObject{

    const float BOX_SCALE_X = 0.2f, BOX_SCALE_Y= 0.2f, BOX_SCALE_Z= 0.2f;
    const float BOX_DIST = 2.0f;
    const float LANDING_SCALE_REDUCTION_FACTOR = 0.015f;
    Mediator* p_mediator;

    btVector3 angularVelocity; //set by asteroid obj and used by lander, basically just passing through here for convenience

    LandingSiteObj(Mediator* mediator): p_mediator{mediator}{}

    void constructLandingSite(SceneData& sceneData, 
            std::vector<std::shared_ptr<WorldObject>>* objects,
            std::vector<std::shared_ptr<RenderObject>>* renderableObjects,
            MyScene* myScene
            ){
        //first get center of scenario landing site, scale it, 
        //reduce scaling by a factor, helps with overshooting the landing site pos, not great still though
        //instead i should move it down towards origin by an amount determined by scaleAmount
        float scaleAmount = sceneData.ASTEROID_SCALE*LANDING_SCALE_REDUCTION_FACTOR;
        glm::mat4 scale_m = glm::scale(glm::mat4(1.0f), glm::vec3(sceneData.ASTEROID_SCALE, sceneData.ASTEROID_SCALE, sceneData.ASTEROID_SCALE)); //scale matrix
        glm::vec3 towardsOrigin = glm::normalize(-sceneData.landingSite.pos)*scaleAmount;
        glm::vec3 landingSitePos = scale_m * glm::vec4(sceneData.landingSite.pos+towardsOrigin,1);
        glm::mat4 landingSiteRot = sceneData.landingSite.rot;

        pos = landingSitePos;
        initialPos = landingSitePos;
        rot = landingSiteRot;
        initialRot = landingSiteRot;

        //this is only used when manually rotating the landing site
        yaw = sceneData.landingSite.yaw;
        pitch = sceneData.landingSite.pitch;
        roll = sceneData.landingSite.roll;

        //we have created overall landing site object
        //now for the markers, these are placed around landing site in a square
        for(int x = 0; x < 4; x++){
            glm::vec3 landingBoxPos;
            switch (x){
                case 0:landingBoxPos = landingSitePos+glm::vec3(-BOX_DIST,-BOX_DIST,0);break;
                case 1:landingBoxPos = landingSitePos+glm::vec3(BOX_DIST,-BOX_DIST,0);break;
                case 2:landingBoxPos = landingSitePos+glm::vec3(-BOX_DIST,BOX_DIST,0);break;
                case 3:landingBoxPos = landingSitePos+glm::vec3(BOX_DIST,BOX_DIST,0);break;
                default:break;
            }

            //we need to account for landing site rotation before getting our final box position
            //we start with our offset landingBoxPos, make a translation matrix of our landingBoxPos, and the inverse of it
            //then we construct the whole matrix, (remember to read in reverse order)
            //ie we move the point back to 0,0,0 (-landingSitePos), rotate it around origin by landingSiteRot, then translate back to our starting position (+landingSitePos)
            glm::mat4 translate = glm::translate(glm::mat4(1), landingSitePos);
            glm::mat4 invTranslate = glm::inverse(translate);
            glm::mat4 objectRotationTransform = translate * landingSiteRot * invTranslate;
            landingBoxPos = objectRotationTransform*glm::vec4(landingBoxPos, 1);

            std::shared_ptr<RenderObject> landingBox = std::shared_ptr<RenderObject>(new RenderObject());
            landingBox->id = id++;
            landingBox->pos = landingBoxPos;
            landingBox->initialPos = landingBoxPos;
            landingBox->rot = landingSiteRot;
            landingBox->initialRot = landingSiteRot;
            landingBox->scale = glm::vec3(BOX_SCALE_X,BOX_SCALE_Y,BOX_SCALE_Z);
            myScene->setRendererMeshVars("box", landingBox.get());
            landingBox->material = p_mediator->renderer_getMaterial("unlitmesh");
            landingBox->material->extra.x = 2048;
            landingBox->material->diffuse = glm::vec3(0,0.0f,1.0f);
            landingBox->material->specular = glm::vec3(0.5f,0.5f,0.5f);
            landingBox->material->extra.x = 32;

            objects->push_back(landingBox);
            renderableObjects->push_back(landingBox);
        }
    }

    void updateLandingSiteObjects(){
        //get LandingSite WorldObject too and update that first
        WorldObject& asteroidRenderObject = p_mediator->scene_getFocusableObject("Asteroid");
        glm::vec3 rotated_point = asteroidRenderObject.rot * glm::vec4(initialPos, 1);
        pos = rotated_point;
        rot = asteroidRenderObject.rot*initialRot;
        
        for(int x = 0; x < 4; x++){
            WorldObject& box = p_mediator->scene_getWorldObject(4+x); //4 is the first index of landing box in the world object list
            glm::vec3 directionToOrigin = glm::normalize(-initialPos);
            glm::vec3 rotated_point = asteroidRenderObject.rot * glm::vec4(box.initialPos, 1);
            box.pos = rotated_point;
            box.rot = asteroidRenderObject.rot*box.initialRot;
        }

        glm::mat4 scaleM = glm::scale(glm::mat4{ 1.0 }, scale);
        glm::mat4 translation = glm::translate(glm::mat4{ 1.0 }, pos);
        glm::mat4 rotation = rot;
        glm::mat4 landerWorldTransform = translation * rotation * scaleM;
        int indexUpAxis = 2;
        up = glm::normalize(landerWorldTransform[2]);

        int indexFwdAxis = 0;
        forward = glm::normalize(landerWorldTransform[0]);    
    }

    //this function is just to help find locations for the landing site  manually move it in sim, update the render object position or rotation and print out the pos and rot
    void moveLandingSite(float x, float y, float z, bool torque){
        if(torque){
            //input 
            float step = 1.0f;
            yaw+=x*step;
            pitch+=y*step;
            roll+=z*step;

            initialRot = glm::yawPitchRoll(glm::radians(yaw), glm::radians(pitch), glm::radians(roll));

            for(int x = 0; x < 4; x++){
                WorldObject& box = p_mediator->scene_getWorldObject(4+x); //4 is the first index of landing box in the world object list
                box.initialRot = initialRot;

                glm::vec3 landingBoxPos;
                switch (x){
                    case 0:landingBoxPos = pos+glm::vec3(-1,-1,0);break;
                    case 1:landingBoxPos = pos+glm::vec3(1,-1,0);break;
                    case 2:landingBoxPos = pos+glm::vec3(-1,1,0);break;
                    case 3:landingBoxPos = pos+glm::vec3(1,1,0);break;
                    default:break;
                }

                //we need to account for landing site rotation before getting our final box position
                //we start with our offset landingBoxPos, make a translation matrix of our landingBoxPos, and the inverse of it
                //then we construct the whole matrix, (remember to read in reverse order)
                //ie we move the point back to 0,0,0 (-landingSitePos), rotate it around origin by landingSiteRot, then translate back to our starting position (+landingSitePos)
                //also used in dmn_myScene, could move to Service/helper functions
                glm::mat4 translate = glm::translate(glm::mat4(1), pos);
                glm::mat4 invTranslate = glm::inverse(translate);
                glm::mat4 objectRotationTransform = translate * initialRot * invTranslate;
                //box.pos = objectRotationTransform*glm::vec4(landingBoxPos, 1);
                box.initialPos = objectRotationTransform*glm::vec4(landingBoxPos, 1);
            }
        }
        else{
            glm::vec3 correctedDirection = rot * glm::vec4(x, y, z, 0);
            pos += correctedDirection/10.0f;
            initialPos = pos;
            for(int x = 0; x < 4; x++){
                WorldObject& box = p_mediator->scene_getWorldObject(4+x); //4 is the first index of landing box in the world object list
                box.pos += correctedDirection/10.0f;
                box.initialPos = box.pos;
            }
        } 

        std::cout << "Landing Site Moved: " << "\n";
        std::cout << glm::to_string(initialPos) << "\n";
        std::cout << yaw  << ", " << pitch << ", " << roll << "\n";
    }
};