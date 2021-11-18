#include "dmn_myScene.h"
#include "obj_collisionRender.h"
#include "obj_lander.h"
#include "obj_asteroid.h"
#include "sv_randoms.h"
#include <glm/gtx/string_cast.hpp>
#include "obj_landingSite.h"

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
    r_mediator.renderer_allocateDescriptorSetForTexture("texturedmesh2", "satellite");
    r_mediator.renderer_allocateDescriptorSetForSkybox();

    r_mediator.renderer_mapMaterialDataToGPU(); //important to do after textures/materials are loaded

    //r_mediator.renderer_allocateShadowMapImages(); //uses LightPointers as set above, iterates through and creates VkImage for each
}

void MyScene::configurePhysicsEngine(){
    r_mediator.physics_reset();
    r_mediator.physics_loadCollisionMeshes(&collisionObjects);
    //r_mediator.physics_initDynamicsWorld();
    r_mediator.physics_updateDeltaTime();
}

//this is temporary fix, bad object design and logic caused this
void MyScene::setRendererMeshVars(std::string name, RenderObject* renderObj){
    Mesh* mesh = r_mediator.renderer_getLoadedMesh(name);
    renderObj->meshId = mesh->id;
    renderObj->indexBase = mesh->indexBase;
    renderObj->indexCount = mesh->indexCount;
}

void MyScene::initObjects(){ 
    //need to use lists instead? because they wont break pointer references when adding or removing elements
    objects.clear();
    renderableObjects.clear();
    collisionObjects.clear();
    focusableObjects.clear();
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
    starSphere->pos = glm::vec3(-9000,0,0);
    starSphere->scale = glm::vec3(100,100,100);
    setRendererMeshVars("sphere", starSphere.get());
    starSphere->material = r_mediator.renderer_getMaterial("unlitmesh");
    starSphere->material->diffuse = glm::vec3(1,0.5f,0.31f);
    starSphere->material->specular = glm::vec3(0.5f,0.5f,0.5f);
    starSphere->material->extra.x = 32;
    objects.push_back(starSphere);
    renderableObjects.push_back(starSphere);

    lander = std::shared_ptr<LanderObj>(new LanderObj());
    lander->id = id++;
    if(sceneData.RANDOMIZE_START){ //best to update this now
        glm::vec3 pos = Service::bt2glm(Service::getPointOnSphere(Service::getRandFloat(30,150), Service::getRandFloat(0,360), sceneData.LANDER_START_DISTANCE));
        lander->pos = pos;
    }
    else
        lander->pos = Service::bt2glm(Service::getPointOnSphere(0, 0, sceneData.LANDER_START_DISTANCE));

    lander->scale = glm::vec3(1.0f,1.0f,1.0f); //lander aka box
    setRendererMeshVars("box", lander.get());
    lander->material = r_mediator.renderer_getMaterial("defaultmesh");
    lander->material->diffuse = glm::vec3(0,0,1);
    lander->material->specular = glm::vec3(1,0.5f,0.5f);
    lander->material->extra.x = 32;
    lander->mass = 1.0f;

    lander->collisionCourse = sceneData.LANDER_COLLISION_COURSE;
    lander->randomStartPositions = sceneData.RANDOMIZE_START;
    lander->asteroidGravForceMultiplier = sceneData.GRAVITATIONAL_FORCE_MULTIPLIER;
    lander->startDistance = sceneData.LANDER_START_DISTANCE;
    lander->passDistance = sceneData.LANDER_PASS_DISTANCE;
    lander->initialSpeed = sceneData.INITIAL_LANDER_SPEED;

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
    asteroid->material->extra.x = 2048;
    asteroid->mass = 100000;

    if(sceneData.RANDOMIZE_START){ //easier if we do this now, then we can pass angular velocity to landing site obj, so lander can get it, simple :)
        float f = sceneData.ASTEROID_MAX_ROTATIONAL_VELOCITY/sceneData.ASTEROID_SCALE;
        asteroid->angularVelocity = btVector3(Service::getRandFloat(-f,f),Service::getRandFloat(-f,f),Service::getRandFloat(-f,f));
        //asteroid->angularVelocity = btVector3(0.0f, 0.0f, Service::getRandFloat(-f,f));
    }
    else{
        asteroid->angularVelocity = btVector3(0.0f, 0.0f, 0.0f);
    }
    //asteroid->randomStartRotation = sceneData.RANDOMIZE_START;
    //asteroid->maxRotationVelocity = sceneData.ASTEROID_MAX_ROTATIONAL_VELOCITY/sceneData.ASTEROID_SCALE;
    //std::cout << asteroid->maxRotationVelocity << " max rotational velocity\n";

    objects.push_back(asteroid);
    renderableObjects.push_back(asteroid);   
    collisionObjects.push_back(asteroid);
    focusableObjects["Asteroid"] = asteroid;

    landingSite = std::shared_ptr<LandingSiteObj>(new LandingSiteObj(&r_mediator));
    landingSite.get()->constructLandingSite(sceneData, &objects, &renderableObjects, this);
    focusableObjects["Landing_Site"] = landingSite;
    landingSite->angularVelocity = asteroid->angularVelocity; //for convenience
}

void MyScene::initLights(){
    //set directional scene light values
    //this is light from the star in the background
    sceneLight.pos = glm::vec3(1,0,0);
    sceneLight.ambient = glm::vec3(0.025f,0.025f,0.1f);
    sceneLight.diffuse = glm::vec3(0.8f,0.8f,0.25f);

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
    spotlight.initialPos = {0,0,-1};
    spotlight.scale = {0.05f,0.05f,0.05f};
    spotlight.diffuse = {1,1,1};
    spotlight.specular = {1,1,1};
    //spotlight.attenuation = {1.0f,0.07f,0.017f};
    spotlight.attenuation = {0.1f,0.005f,0.001f};
    spotlight.direction = glm::vec3(0,0,1);
    spotlight.cutoffAngles = glm::vec2(0.75f, 1.2f);
    spotlight.cutoffs = {glm::cos(spotlight.cutoffAngles.x), glm::cos(spotlight.cutoffAngles.y)};
    spotLights.push_back(spotlight);

    lander.get()->p_spotlight = &spotLights.at(0); //hacky, our lander needs the world spotlight obj to update its pos and direction each frame
    //however this shouldnt be controlled by lander timestep because its unnecessary to update it until we want to draw it, ie physics engine doesnt matter
}