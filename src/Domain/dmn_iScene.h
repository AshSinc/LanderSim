#pragma once
#include <vector>

#include "obj_light.h"
#include "obj_pointLight.h"
#include "obj_spotLight.h"

#include <map>
#include <memory>
#include <string>

class WorldObject;
class CollisionRenderObj;
class RenderObject;

struct ModelInfo{
    std::string modelName;
    std::string filePath;
};

struct TextureInfo{
    std::string textureName;
    std::string filePath;
};
   
class IScene{
    protected:
        WorldLightObject sceneLight;
        std::vector<WorldPointLightObject> pointLights;
        std::vector<WorldSpotLightObject> spotLights;

        std::vector<std::shared_ptr<WorldObject>> objects;
        std::vector<std::shared_ptr<CollisionRenderObj>> collisionObjects;
        std::vector<std::shared_ptr<RenderObject>> renderableObjects;

    public:
        int getNumPointLights(){return pointLights.size();}
        int getNumSpotLights(){return spotLights.size();}
        int getWorldObjectsCount(){return objects.size();}
        WorldObject& getWorldObject(int i){return *objects.at(i);}
        virtual void initScene() = 0;
};
