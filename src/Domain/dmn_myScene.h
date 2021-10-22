#pragma once
#include "dmn_iScene.h"
#include "mediator.h"
#include <string>
#include "data_scene.h"


class MyScene: public IScene{
        public:
        MyScene(Mediator& mediator);
        void initScene(SceneData sceneData);
        
        SceneData sceneData;

        void setSceneData(SceneData sceneData);
        void setRendererMeshVars(std::string name, RenderObject* renderObj);
        LandingSiteObj* getLandingSiteObject(){return landingSite.get();};

        private:
        void initLights();
        void initObjects();
        void initRenderables();
        void configureRenderEngine();
        void configurePhysicsEngine();
        
        glm::mat4 rotation_from_euler(double roll, double pitch, double yaw);
        Mediator& r_mediator;

        std::shared_ptr<LandingSiteObj> landingSite;

        //model identifier and path pairs, for assigning to unnordered map, loading code needs cleaned and moved
        const std::vector<ModelInfo> MODEL_INFOS = {
            {"satellite", "resources/models/Satellite.obj"},
            //{"asteroid", "resources/models/asteroid.obj"},
            {"asteroid", "resources/models/asteroidscaled.obj"},
            //{"asteroid", "models/Bennu.obj"},
            {"sphere", "resources/models/sphere.obj"},
            {"box", "resources/models/box.obj"}
        };

        //texture identifier and path pairs, used in loading and assigning to map
        const std::vector<TextureInfo> TEXTURE_INFOS = {
            {"satellite_diff", "resources/textures/Satellite.jpg"},
            {"satellite_spec", "resources/textures/Satellite_Specular.jpg"},
            {"asteroid_diff", "resources/textures/ASTEROID_COLOR.jpg"},
            {"asteroid_spec", "resources/textures/ASTEROID_SPECULAR.jpg"}
            //{"asteroid_diff", "resources/textures/ASTEROID_COLORB.jpg"},
            //{"asteroid_spec", "resources/textures/ASTEROID_COLORB.jpg"}
        };

        //texture identifier and path pairs, used in loading and assigning to map
        const std::vector<std::string> SKYBOX_PATHS = {
            {"resources/textures/skybox/GalaxyTex_PositiveX.jpg"},
            {"resources/textures/skybox/GalaxyTex_NegativeX.jpg"},
            {"resources/textures/skybox/GalaxyTex_PositiveZ.jpg"},
            {"resources/textures/skybox/GalaxyTex_NegativeZ.jpg"},
            {"resources/textures/skybox/GalaxyTex_NegativeY.jpg"},
            {"resources/textures/skybox/GalaxyTex_PositiveY.jpg"}
        };
};
