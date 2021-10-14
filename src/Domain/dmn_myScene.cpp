#include "dmn_myScene.h"
#include "obj_collisionRender.h"
#include "obj_lander.h"
#include "obj_asteroid.h"
#include "sv_randoms.h"
#include <glm/gtx/string_cast.hpp>

MyScene::MyScene(Mediator& mediator): r_mediator{mediator}{}

void MyScene::setSceneData(SceneData isceneData){
    sceneData = isceneData;
}

void MyScene::initScene(SceneData latestSceneData){
    sceneData = latestSceneData;
    r_mediator.ui_updateLoadingProgress(0.1f, "loading models...");
    //this this is crashing sometimes when running on seperate thread, need to thread safe renderer at least
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

    std::shared_ptr<LanderObj> lander = std::shared_ptr<LanderObj>(new LanderObj());
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
    lander->mass = 1;

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
    //asteroid.material->extra.x = 32;
    asteroid->randomStartRotation = sceneData.RANDOMIZE_START;
    asteroid->maxRotationVelocity = sceneData.ASTEROID_MAX_ROTATIONAL_VELOCITY;

    objects.push_back(asteroid);
    renderableObjects.push_back(asteroid);   
    collisionObjects.push_back(asteroid);
    focusableObjects["Asteroid"] = asteroid;

    constructLandingSite(id);
}

void MyScene::constructLandingSite(int id){
    //first get center of scenario landing site, scale it, 
    //reduce scaling by a factor, helps with overshooting the landing site pos, not great still though
    //instead i should move it down towards origin by an amount determined by scaleAmount
    float scaleAmount = sceneData.ASTEROID_SCALE*LANDING_SCALE_REDUCTION_FACTOR;
    glm::mat4 scale_m = glm::scale(glm::mat4(1.0f), glm::vec3(sceneData.ASTEROID_SCALE, sceneData.ASTEROID_SCALE, sceneData.ASTEROID_SCALE)); //scale matrix
    glm::vec3 towardsOrigin = glm::normalize(-sceneData.landingSite.pos)*scaleAmount;
    glm::vec3 landingSitePos = scale_m * glm::vec4(sceneData.landingSite.pos+towardsOrigin,1);
    glm::mat4 landingSiteRot = sceneData.landingSite.rot;
    std::cout << glm::to_string(sceneData.landingSite.rot) << "\n";
    std::cout << glm::to_string(landingSiteRot) << "\n";

    std::shared_ptr<WorldObject> landingSite = std::shared_ptr<WorldObject>(new WorldObject());

    landingSite->pos = landingSitePos;
    landingSite->initialPos = landingSitePos;
    landingSite->rot = landingSiteRot;
    landingSite->initialRot = landingSiteRot;

    //this is only used when manually rotating the landing site
    landingSite->yaw = sceneData.landingSite.yaw;
    landingSite->pitch = sceneData.landingSite.pitch;
    landingSite->roll = sceneData.landingSite.roll;

    focusableObjects["Landing_Site"] = landingSite;

    //we have created overall landing site object
    //now for the markers, these are placed around landing site in a square
    for(int x = 0; x < 4; x++){
        glm::vec3 landingBoxPos;
        switch (x){
            case 0:landingBoxPos = landingSitePos+glm::vec3(-1,-1,0);break;
            case 1:landingBoxPos = landingSitePos+glm::vec3(1,-1,0);break;
            case 2:landingBoxPos = landingSitePos+glm::vec3(-1,1,0);break;
            case 3:landingBoxPos = landingSitePos+glm::vec3(1,1,0);break;
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
        landingBox->scale = glm::vec3(0.1f,0.1f,0.1f);
        setRendererMeshVars("box", landingBox.get());
        landingBox->material = r_mediator.renderer_getMaterial("unlitmesh");
        landingBox->material->extra.x = 2048;
        landingBox->material->diffuse = glm::vec3(0,1.0f,0);
        landingBox->material->specular = glm::vec3(0.5f,0.5f,0.5f);
        landingBox->material->extra.x = 32;

        objects.push_back(landingBox);
        renderableObjects.push_back(landingBox);
    }
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
    spotlight.pos = {4000,0,-5};
    spotlight.scale = {0.05f,0.05f,0.05f};
    spotlight.diffuse = {0,1,1};
    spotlight.specular = {1,1,1};
    spotlight.attenuation = {1,0.07f,0.017f};
    spotlight.direction = glm::vec3(4010,50,-5) - spotlight.pos; //temp direction
    spotlight.cutoffAngles = glm::vec2(10.0f, 25.0f);
    spotlight.cutoffs = {glm::cos(spotlight.cutoffAngles.x), glm::cos(spotlight.cutoffAngles.y)};
    std::cout << spotlight.cutoffs.x << "\n";
    std::cout << spotlight.cutoffs.y << "\n";
    spotLights.push_back(spotlight);

    spotLights.push_back(spotlight);
}