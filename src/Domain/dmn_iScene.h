#pragma once
#include <vector>

#include "obj_light.h"
#include "obj_pointLight.h"
#include "obj_spotLight.h"
#include "obj_render.h"

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
    
        std::vector<WorldObject> objects{}; //init default
        std::vector<RenderObject&> renderableObjects{};

    public:
        IScene();
        int getNumPointLights(){return pointLights.size();}
        int getNumSpotLights(){return spotLights.size();}
        virtual void initScene() = 0;
};
