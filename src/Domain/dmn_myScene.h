#pragma once
#include "dmn_iScene.h"
#include "mediator.h"
#include <string>

class MyScene: public IScene{
        public:
        MyScene(Mediator& mediator);
        void initScene();
        private:
        
        bool RANDOMIZE_START = true;
        bool LANDER_COLLISION_COURSE = false;

        const float GRAVITATIONAL_FORCE_MULTIPLIER = 0.5f;
        const float LANDER_START_DISTANCE = 150.0f;
        const float LANDER_PASS_DISTANCE = 70.0f;
        const float INITIAL_LANDER_SPEED = 1.5f;

        const float ASTEROID_MAX_ROTATIONAL_VELOCITY = 0.03f;

        void initLights();
        void initObjects();
        void initRenderables();
        void configureRenderEngine();
        void configurePhysicsEngine();
        void setRendererMeshVars(std::string name, RenderObject* renderObj);
        Mediator& r_mediator;

        //model identifier and path pairs, for assigning to unnordered map, loading code needs cleaned and moved
        const std::vector<ModelInfo> MODEL_INFOS = {
            {"satellite", "resources/models/Satellite.obj"},
            {"asteroid", "resources/models/asteroid.obj"},
            //{"asteroid", "models/Bennu.obj"},
            {"sphere", "resources/models/sphere.obj"},
            {"box", "resources/models/box.obj"}
        };

        //texture identifier and path pairs, used in loading and assigning to map
        const std::vector<TextureInfo> TEXTURE_INFOS = {
            {"satellite_diff", "resources/textures/Satellite.jpg"},
            {"satellite_spec", "resources/textures/Satellite_Specular.jpg"},
            //{"asteroid_diff", "textures/ASTEROID_COLOR.jpg"},
            //{"asteroid_spec", "textures/ASTEROID_SPECULAR.jpg"}
            {"asteroid_diff", "resources/textures/ASTEROID_COLORB.jpg"},
            {"asteroid_spec", "resources/textures/ASTEROID_COLORB.jpg"}
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
