#include "dmn_myScene.h"
#include "obj_collisionRender.h"
#include "obj_lander.h"
#include "obj_asteroid.h"
#include "sv_randoms.h"
#include <glm/gtx/string_cast.hpp>
#include "obj_landingSite.h"
#include <glm/gtx/quaternion.hpp>

MyScene::MyScene(Mediator& mediator): r_mediator{mediator}{}

void MyScene::setSceneData(SceneData isceneData){
    sceneData = isceneData;
}

void MyScene::initScene(SceneData latestSceneData){
    sceneData = latestSceneData;
    r_mediator.ui_updateLoadingProgress(0.1f, "loading models...");
    r_mediator.renderer_loadModels(MODEL_INFOS);
    r_mediator.ui_updateLoadingProgress(0.25f, "loading textures...");
    r_mediator.renderer_loadTextures(TEXTURE_INFOS, SKYBOX_PATHS);
    r_mediator.ui_updateLoadingProgress(0.5f, "initialise world objects...");
    initObjects();
    r_mediator.ui_updateLoadingProgress(0.6f, "initialise light objects...");
    initLights();
    r_mediator.ui_updateLoadingProgress(0.7f, "configure render engine...");
    configureRenderEngine();
    r_mediator.ui_updateLoadingProgress(0.8f, "configure physics engine...");
    configurePhysicsEngine();
    r_mediator.ui_updateLoadingProgress(1.0f, "finalise...");
}

void MyScene::configureRenderEngine(){
    r_mediator.renderer_setRenderablesPointer(&renderableObjects);
    r_mediator.renderer_setLightPointers(&sceneLight, &pointLights, &spotLights);
    r_mediator.renderer_setCameraDataPointer(r_mediator.camera_getCameraDataPointer());

    r_mediator.renderer_allocateDescriptorSetForTexture("texturedmesh1", "asteroid");
    r_mediator.renderer_allocateDescriptorSetForTexture("greyscale_texturedmesh1", "asteroid");
    r_mediator.renderer_allocateDescriptorSetForTexture("texturedmesh2", "lander");
    r_mediator.renderer_allocateDescriptorSetForSkybox();

    r_mediator.renderer_mapMaterialDataToGPU(); //important to do after textures/materials are loaded

    //r_mediator.renderer_allocateShadowMapImages(); //uses LightPointers as set above, iterates through and creates VkImage for each
}

void MyScene::configurePhysicsEngine(){
    r_mediator.physics_reset();
    r_mediator.physics_loadCollisionMeshes(&collisionObjects);
    r_mediator.physics_updateDeltaTime();
}

//setting renderer mesh variables and binding to name, used in vulkan render passes, ideally should be moved
void MyScene::setRendererMeshVars(std::string name, RenderObject* renderObj){
    Mesh* mesh = r_mediator.renderer_getLoadedMesh(name);
    renderObj->meshId = mesh->id;
    renderObj->indexBase = mesh->indexBase;
    renderObj->indexCount = mesh->indexCount;
}

void MyScene::initObjects(){ 
    objects.clear();
    renderableObjects.clear();
    collisionObjects.clear();
    focusableObjects.clear();
    debugObjects.clear();
    int id = 0;

    std::shared_ptr<RenderObject> skybox = std::shared_ptr<RenderObject>(new RenderObject());
    skybox->id = id++;
    skybox->pos = glm::vec3(0,0,0);
    skybox->scale = glm::vec3(10000,10000,10000);
    setRendererMeshVars("box", skybox.get());
    objects.push_back(skybox);
    renderableObjects.push_back(skybox);

    std::shared_ptr<RenderObject> starSphere = std::shared_ptr<RenderObject>(new RenderObject());
    starSphere->id = id++;
    starSphere->pos = glm::vec3(0,0,9000);
    starSphere->scale = glm::vec3(100,100,100);
    setRendererMeshVars("sphere", starSphere.get());
    starSphere->material = r_mediator.renderer_getMaterial("unlitmesh");
    starSphere->material->diffuse = glm::vec3(1,0.5f,0.31f);
    starSphere->material->specular = glm::vec3(0.5f,0.5f,0.5f);
    starSphere->material->extra.x = 32;
    starSphere->altMaterial = r_mediator.renderer_getMaterial("greyscale_unlitmesh");
    starSphere->altMaterial->diffuse = glm::vec3(1,0.5f,0.31f);
    starSphere->altMaterial->specular = glm::vec3(0.5f,0.5f,0.5f);
    starSphere->altMaterial->extra.x = 32;
    objects.push_back(starSphere);
    renderableObjects.push_back(starSphere);

    lander = std::shared_ptr<LanderObj>(new LanderObj());
    lander->id = id++;
    lander->scale = glm::vec3(1.0f,1.0f,1.0f);
    setRendererMeshVars("lander", lander.get());   //instead of box and default mesh we need to assign the model
    lander->material = r_mediator.renderer_getMaterial("texturedmesh2");
    lander->material->extra.x = 64;
    lander->mass = 1.0f;

    lander->asteroidGravForceMultiplier = sceneData.GRAVITATIONAL_FORCE_MULTIPLIER;
    lander->startDistance = sceneData.LANDER_START_DISTANCE;
    lander->useEstimateOnly = sceneData.USE_ONLY_ESTIMATE;

    objects.push_back(lander);
    renderableObjects.push_back(lander);
    collisionObjects.push_back(lander);
    focusableObjects["Lander"] = lander;

    std::shared_ptr<AsteroidObj> asteroid = std::shared_ptr<AsteroidObj>(new AsteroidObj());
    asteroid->id = id++;
    asteroid->pos = glm::vec3(0,0,0);
    asteroid->scale = glm::vec3(sceneData.ASTEROID_SCALE, sceneData.ASTEROID_SCALE, sceneData.ASTEROID_SCALE);
    setRendererMeshVars("asteroid", asteroid.get());
    asteroid->material = r_mediator.renderer_getMaterial("texturedmesh1");
    asteroid->material->extra.x = 32;
    asteroid->altMaterial = r_mediator.renderer_getMaterial("greyscale_texturedmesh1"); //greyscale
    asteroid->altMaterial->extra.x = 32;
    asteroid->mass = 100000;

    asteroid->angularVelocity = btVector3(0.0f, 0.0f, 0.0f);
    asteroid->initialRotation.setEulerZYX(0,0,0);
    asteroid->initialRot = glm::toMat4(Service::bulletToGlm(asteroid->initialRotation));

    if(sceneData.RANDOMIZE_ROTATION){ //easier if we do this now, then we can pass angular velocity to landing site obj, so lander can get it, simple :)
        float f = sceneData.ASTEROID_MAX_ROTATIONAL_VELOCITY/sceneData.ASTEROID_SCALE;
        int axis = Service::getRandFloat(0,3);
        float angle = Service::getAbsMax(Service::getRandFloat(-f,f), sceneData.ASTEROID_MIN_ROTATIONAL_VELOCITY*sceneData.ASTEROID_SCALE);
        switch (axis){
        case 0:
            asteroid->angularVelocity = btVector3(angle, 0.0f, 0.0f);
            break;
        case 1:
            asteroid->angularVelocity = btVector3(0.0f, angle, 0.0f);
            break;
        default:
        asteroid->angularVelocity = btVector3(0.0f, 0.0f, angle);
            break;
        }

        //ISSUE more than one axis of rotation also messes things up
        //suspect it may just be my descent checking predictAtTf rotations, because they wont account for angular accelrations over time maybe?
        //asteroid->angularVelocity = btVector3(Service::getRandFloat(-f,f), Service::getRandFloat(-f,f), Service::getRandFloat(-f,f));

        //ISSUE randomizing starting rotation messes up descent checks in lander ai, need to account for initial rotation in there
        //possibly in other locations too, like the zemzev gnc and maybe even landing site?
        //asteroid->initialRotation.setEulerZYX(Service::getRandFloat(0,359),Service::getRandFloat(0,359),Service::getRandFloat(0,359));
        
    }
    else{
        asteroid->angularVelocity = btVector3(sceneData.ASTEROID_ROTATION_X, sceneData.ASTEROID_ROTATION_Y, sceneData.ASTEROID_ROTATION_Z);
    }

    objects.push_back(asteroid);
    renderableObjects.push_back(asteroid);   
    collisionObjects.push_back(asteroid);
    focusableObjects["Asteroid"] = asteroid;

    landingSite = std::shared_ptr<LandingSiteObj>(new LandingSiteObj(&r_mediator));
    landingSite.get()->constructLandingSite(sceneData, &objects, &renderableObjects, this);
    focusableObjects["Landing_Site"] = landingSite;
    landingSite->angularVelocity = asteroid->angularVelocity; //for convenience

    //try to start around 1000m altitude with optics fov scaled by the asteroid size
    lander->pos = glm::vec3(0, 0, sceneData.LANDER_START_DISTANCE+(landingSite->pos.z));
    lander->asteroidScale = sceneData.ASTEROID_SCALE;

    if(Service::OUTPUT_TEXT){
        //output scenario data to file, shouldn't really be here but all the data is here so...
        r_mediator.writer_writeToFile("PARAMS", "UseEstimateOnly:" + std::to_string(sceneData.USE_ONLY_ESTIMATE));
        r_mediator.writer_writeToFile("PARAMS", "Scale:" + std::to_string(sceneData.ASTEROID_SCALE));
        r_mediator.writer_writeToFile("PARAMS", "AngularVelocity:" + glm::to_string(Service::bt2glm(asteroid->angularVelocity)));
        r_mediator.writer_writeToFile("PARAMS", "LanderStartPos:" + glm::to_string(lander->pos));
        r_mediator.writer_writeToFile("PARAMS", "GravityMultiplier:" + std::to_string(lander->asteroidGravForceMultiplier));
    }
}

void MyScene::initLights(){
    //set directional scene light values
    //this is light from the star in the background
    sceneLight.pos = glm::vec3(0,0,-1); //above
    //sceneLight.pos = glm::vec3(0,0.5,-0.5); //45 degree
    //sceneLight.pos = glm::vec3(-1,0,0); //90 degree
    sceneLight.ambient = glm::vec3(0.01f,0.01f,0.01f); //<-- this is good for in engine
    //sceneLight.ambient = glm::vec3(0.00f,0.00f,0.00f);
    sceneLight.diffuse = glm::vec3(0.2f,0.2f,0.2f);
    sceneLight.specular = glm::vec3(2.0f,2.0f,2.0f);

    /*WorldPointLightObject pointLight;
    pointLight.pos = glm::vec3(4005,0,-5);
    pointLight.scale = glm::vec3(0.2f, 0.2f, 0.2f);
    pointLight.ambient = glm::vec3(0.9f,0.025f,0.1f);
    pointLight.diffuse = glm::vec3(0.9f,0.6f,0.5f);
    pointLight.specular = glm::vec3(0.8f,0.5f,0.5f);
    pointLight.attenuation = glm::vec3(1, 0.7f, 1.8f);
    pointLights.push_back(pointLight);*/

    //add a spot light object
    //this will be a spotlight on the lander
    WorldSpotLightObject spotlight;
    spotlight.pos = {0,0,0};
    spotlight.initialPos = {0,0,-0.5};
    spotlight.diffuse = {1,1,1};
    spotlight.specular = {1,1,1};
    spotlight.attenuation = {0.2f,0.005f,0.001f};
    //spotlight.attenuation = {0.1f,0.0005f,0.001f};
    spotlight.direction = glm::vec3(0,0,1);
    //spotlight.cutoffAngles = glm::vec2(0.75f, 1.2f);
    //spotlight.cutoffAngles = glm::vec2(0.75f, 3.2f);
    //spotlight.cutoffAngles = glm::vec2(0.05f, 15.0f);
    spotlight.cutoffAngles = glm::vec2(0.01f, 0.50f);
    spotlight.cutoffs = {glm::cos(spotlight.cutoffAngles.x), glm::cos(spotlight.cutoffAngles.y)};
    spotLights.push_back(spotlight);

    lander.get()->p_spotlight = &spotLights.at(0); //hacky, our lander needs the world spotlight obj to update its pos and direction each frame
    //however this shouldnt be controlled by lander timestep because its unnecessary to update it until we want to draw it, ie physics engine doesnt matter

    if(Service::OUTPUT_TEXT){
        r_mediator.writer_writeToFile("PARAMS", "LightDir:" + glm::to_string(sceneLight.pos));
    }
}