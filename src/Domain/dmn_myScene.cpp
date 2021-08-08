#include "dmn_myScene.h"
#include "obj_collisionRender.h"
#include "obj_lander.h"
#include "obj_asteroid.h"

MyScene::MyScene(Mediator& mediator): r_mediator{mediator}{}

void MyScene::initScene(){
    r_mediator.ui_updateLoadingProgress(0.1f, "loading models...");
    //this this is crashing sometimes when running on seperate thread, need to thread safe renderer at least
    r_mediator.renderer_loadModels(MODEL_INFOS);
    r_mediator.ui_updateLoadingProgress(0.25f, "loading textures...");
    r_mediator.renderer_loadTextures(TEXTURE_INFOS, SKYBOX_PATHS);
    r_mediator.ui_updateLoadingProgress(0.5f, "initialise world objects...");
    initObjects();
    r_mediator.ui_updateLoadingProgress(0.7f, "initialise light objects...");
    initLights();
    r_mediator.ui_updateLoadingProgress(0.8f, "configure render engine...");
    configureRenderEngine();
    r_mediator.ui_updateLoadingProgress(0.9f, "configure physics engine...");
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
}

void MyScene::configurePhysicsEngine(){
    r_mediator.physics_loadCollisionMeshes(&collisionObjects);
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
    starSphere->pos = glm::vec3(-5000,0,0);
    starSphere->scale = glm::vec3(100,100,100);
    setRendererMeshVars("sphere", starSphere.get());
    starSphere->material = r_mediator.renderer_getMaterial("unlitmesh");
    starSphere->material->diffuse = glm::vec3(1,0.5f,0.31f);
    starSphere->material->specular = glm::vec3(0.5f,0.5f,0.5f);
    starSphere->material->extra.x = 32;
    objects.push_back(starSphere);
    renderableObjects.push_back(starSphere);

    std::shared_ptr<LanderObj> lander = std::shared_ptr<LanderObj>(new LanderObj());
    lander->id = id++;
    lander->pos = glm::vec3(75,75,20);
    lander->scale = glm::vec3(0.5f,0.5f,0.5f); //lander aka box
    setRendererMeshVars("box", lander.get());
    lander->material = r_mediator.renderer_getMaterial("defaultmesh");
    lander->material->diffuse = glm::vec3(0,0,1);
    lander->material->specular = glm::vec3(1,0.5f,0.5f);
    lander->material->extra.x = 32;
    lander->mass = 10;

    lander->collisionCourse = LANDER_COLLISION_COURSE;
    lander->randomStartPositions = RANDOMIZE_START;
    lander->asteroidGravForceMultiplier = GRAVITATIONAL_FORCE_MULTIPLIER;
    lander->startDistance = LANDER_START_DISTANCE;
    lander->passDistance = LANDER_PASS_DISTANCE;
    lander->initialSpeed = INITIAL_LANDER_SPEED;

    objects.push_back(lander);
    renderableObjects.push_back(lander);
    collisionObjects.push_back(lander);
    focusableObjects["Lander"] = lander;

    std::shared_ptr<RenderObject> satellite = std::shared_ptr<RenderObject>(new RenderObject());
    satellite->pos = glm::vec3(3900,100,0);
    satellite->scale = glm::vec3(0.2f,0.2f,0.2f);
    setRendererMeshVars("satellite", satellite.get());
    satellite->material = r_mediator.renderer_getMaterial("texturedmesh2");
    satellite->material->extra.x = 2048;
    objects.push_back(satellite);
    renderableObjects.push_back(satellite);

    std::shared_ptr<AsteroidObj> asteroid = std::shared_ptr<AsteroidObj>(new AsteroidObj());
    asteroid->pos = glm::vec3(0,0,0);
    asteroid->scale = glm::vec3(20,20,20);
    setRendererMeshVars("asteroid", asteroid.get());
    asteroid->material = r_mediator.renderer_getMaterial("texturedmesh1");
    asteroid->material->extra.x = 2048;
    asteroid->mass = 10000;
    //asteroid.material->extra.x = 32;
    asteroid->randomStartRotation = RANDOMIZE_START;
    asteroid->maxRotationVelocity = ASTEROID_MAX_ROTATIONAL_VELOCITY;

    objects.push_back(asteroid);
    renderableObjects.push_back(asteroid);   
    collisionObjects.push_back(asteroid);
    focusableObjects["Asteroid"] = asteroid;
}

void MyScene::initLights(){
    //set directional scene light values
    sceneLight.pos = glm::vec3(1,0,0);
    sceneLight.ambient = glm::vec3(0.025f,0.025f,0.1f);
    sceneLight.diffuse = glm::vec3(0.8f,0.8f,0.25f);

    WorldPointLightObject pointLight;
    pointLight.pos = glm::vec3(4005,0,-5);
    pointLight.scale = glm::vec3(0.2f, 0.2f, 0.2f);
    pointLight.ambient = glm::vec3(0.9f,0.025f,0.1f);
    pointLight.diffuse = glm::vec3(0.9f,0.6f,0.5f);
    pointLight.specular = glm::vec3(0.8f,0.5f,0.5f);
    pointLight.attenuation = glm::vec3(1, 0.7f, 1.8f);
    pointLights.push_back(pointLight);

    //add a spot light object
    WorldSpotLightObject spotlight;
    spotlight.pos = {4000,0,-5};
    spotlight.scale = {0.05f,0.05f,0.05f};
    spotlight.diffuse = {0,1,1};
    spotlight.specular = {1,1,1};
    spotlight.attenuation = {1,0.07f,0.017f};
    spotlight.direction = glm::vec3(4010,50,-5) - spotlight.pos; //temp direction
    spotlight.cutoffs = {glm::cos(glm::radians(10.0f)), glm::cos(glm::radians(25.0f))};

    spotLights.push_back(spotlight);
}