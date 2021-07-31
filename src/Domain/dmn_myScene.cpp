#include "dmn_myScene.h"

MyScene::MyScene(Mediator& mediator): r_mediator{mediator}{}

void MyScene::initScene(){
    r_mediator.renderer_loadModels(MODEL_INFOS);
    r_mediator.renderer_loadTextures(TEXTURE_INFOS, SKYBOX_PATHS);
    initObjects();
    //initRenderables();
    initLights();

    configureRenderEngine();
    

}

void MyScene::configureRenderEngine(){
    r_mediator.renderer_setRenderablesPointer(&renderableObjects);

    r_mediator.renderer_setLightPointers(&sceneLight, &pointLights, &spotLights);

    r_mediator.renderer_allocateDescriptorSetForTexture("texturedmesh1", "asteroid");
    r_mediator.renderer_allocateDescriptorSetForTexture("texturedmesh2", "satellite");

    r_mediator.renderer_allocateDescriptorSetForSkybox();

    //r_mediator.renderer_configureMaterialParameters();
}

//loads all scene data, this should be called only when the user hits run
/*void Vk::Renderer::loadScene(){
    //next comes model loading and then creating vertex and index buffers on the gpu and copying the data
    loadModels();
    createVertexBuffer(); //error here if load models later
    createIndexBuffer(); //error here if load models later
    //now we can load the textures and create imageviews
    createTextureImages(); //pretty slow, probs texture number and size
    //creates RenderObjects and associates with WorldObjects, then allocates the textures 
    //this method should be split into two parts createRenderObjects and createTextureDescriptorSets 
    init_scene(); 
    mapMaterialDataToGPU();

    setCameraData(r_mediator.camera_getCameraDataPointer());
}*/

//this all needs moved to myScene objects... with iScene interface
void MyScene::initObjects(){   //defining the materials in here, should have a seperate materials class?
    //renderableObjects.clear();    

    RenderObject skybox;
    skybox.pos = glm::vec3(0,0,0);
    skybox.scale = glm::vec3(10000,10000,10000);
    skybox.meshId = r_mediator.renderer_getMeshId("box");

    //box.material = get_material("defaultmesh");
    //box.material->diffuse = glm::vec3(0,0,1);
    //box.material->specular = glm::vec3(1,0.5f,0.5f);
    //box.material->extra.x = 32;
    objects.push_back(skybox);
    renderableObjects.push_back(skybox);

    RenderObject starSphere;
    starSphere.pos = glm::vec3(-5000,0,0);
    starSphere.scale = glm::vec3(100,100,100);
    starSphere.meshId =  r_mediator.renderer_getMeshId("sphere");
    starSphere.material = r_mediator.renderer_getMaterial("unlitmesh");
    starSphere.material->diffuse = glm::vec3(1,0.5f,0.31f);
    starSphere.material->specular = glm::vec3(0.5f,0.5f,0.5f);
    starSphere.material->extra.x = 32;

    objects.push_back(starSphere);
    renderableObjects.push_back(starSphere);

    RenderObject lander;
    lander.pos = glm::vec3(75,75,20);
    lander.scale = glm::vec3(0.5f,0.5f,0.5f); //lander aka box
    lander.meshId = r_mediator.renderer_getMeshId("box");
    lander.material = r_mediator.renderer_getMaterial("defaultmesh");
    lander.material->diffuse = glm::vec3(0,0,1);
    lander.material->specular = glm::vec3(1,0.5f,0.5f);
    lander.material->extra.x = 32;

    objects.push_back(lander);
    renderableObjects.push_back(lander);

    //make a copy
    /*RenderObject box2{p_worldPhysics->spotLights[0]};
    box2.meshId = get_mesh("box")->id;
    box2.material = get_material("unlitmesh");
    renderableObjects.push_back(box2);*/

    RenderObject satellite;
    satellite.pos = glm::vec3(3900,100,0);
    satellite.scale = glm::vec3(0.2f,0.2f,0.2f);
    satellite.meshId = r_mediator.renderer_getMeshId("satellite");
    satellite.material = r_mediator.renderer_getMaterial("texturedmesh2");
    satellite.material->extra.x = 2048;
    objects.push_back(satellite);
    renderableObjects.push_back(satellite);

    RenderObject asteroid;
    asteroid.pos = glm::vec3(0,0,0);
    asteroid.scale = glm::vec3(20,20,20);
    asteroid.meshId = r_mediator.renderer_getMeshId("asteroid");
    asteroid.material = r_mediator.renderer_getMaterial("texturedmesh1");
    asteroid.material->extra.x = 2048;
    //asteroid.material->extra.x = 32;
    objects.push_back(asteroid);
    renderableObjects.push_back(asteroid);

    

    //allocate a descriptor set for each material pointing to each texture needed
    
}

/*void MyScene::initObjects(){
    //create renderable objects
    renderableObjects.push_back(RenderObject{glm::vec3(0,0,0), glm::vec3(10000,10000,10000)}); //skybox, scale is scene size effectively
    renderableObjects.push_back(RenderObject{glm::vec3(-5000,0,0), glm::vec3(100,100,100)}); //star
    renderableObjects.push_back(CollisionRenderObjObject{glm::vec3(75,75,20), glm::vec3(0.5f,0.5f,0.5f)}); //lander aka box
    objects.push_back(WorldObject{glm::vec3(3900,100,0), glm::vec3(0.2f,0.2f,0.2f)}); //satellite
    objects.push_back(WorldObject{glm::vec3(0,0,0), glm::vec3(20,20,20)}); //asteroid, set to origin

    //objects.push_back(WorldObject{glm::vec3(0,0,0), glm::vec3(10000,10000,10000)}); //skybox, scale is scene size effectively
    //objects.push_back(WorldObject{glm::vec3(-5000,0,0), glm::vec3(100,100,100)}); //star
    //objects.push_back(WorldObject{glm::vec3(75,75,20), glm::vec3(0.5f,0.5f,0.5f)}); //lander aka box
    //objects.push_back(WorldObject{glm::vec3(3900,100,0), glm::vec3(0.2f,0.2f,0.2f)}); //satellite
    //objects.push_back(WorldObject{glm::vec3(0,0,0), glm::vec3(20,20,20)}); //asteroid, set to origin
}*/

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