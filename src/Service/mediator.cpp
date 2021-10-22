#include "mediator.h"
#include "application.h"
#include "vk_renderer_offscreen.h"
#include "dmn_myScene.h"
#include "obj_landingSite.h"
#include "obj_lander.h"

//Scene Functions
int Mediator::scene_getWorldObjectsCount(){
    return p_scene->getWorldObjectsCount();
}
WorldObject& Mediator::scene_getWorldObject(int i){
    return p_scene->getWorldObject(i);
}
WorldObject& Mediator::scene_getFocusableObject(std::string name){
    return p_scene->getFocusableObject(name);
}
LandingSiteObj* Mediator::scene_getLandingSiteObject(){
    MyScene* ls = dynamic_cast<MyScene*>(p_scene);
    return ls->getLandingSiteObject();
}
//LanderObj* Mediator::scene_getLanderObject(){
//    MyScene* ls = dynamic_cast<LanderObj*>(p_scene);
//    return ls->getLandingSiteObject();
//}

//Ui functions
void Mediator::ui_toggleEscMenu(){
    p_uiHandler->toggleMenu();
}
void Mediator::ui_updateUIPanelDimensions(GLFWwindow* window){
    p_uiHandler->updateUIPanelDimensions(window);
}
void Mediator::ui_drawUI(){
    p_uiHandler->drawUI();
}
void Mediator::ui_updateLoadingProgress(float progress, std::string text){
    p_uiHandler->updateLoadingProgress(progress, text);
}

//Camera functions
void Mediator::camera_calculatePitchYaw(double xpos, double ypos){
    p_worldCamera->calculatePitchYaw(xpos, ypos);
}
void Mediator::camera_calculateZoom(double yoffset){
    p_worldCamera->calculateZoom(yoffset);
}
void Mediator::camera_changeFocus(){
    p_worldCamera->changeFocus();
}
void Mediator::camera_setMouseLookActive(bool state){
    p_worldCamera->setMouseLookActive(state);
}
CameraData* Mediator::camera_getCameraDataPointer(){
    return p_worldCamera->getCameraDataPtr();
}
void Mediator::camera_toggleAutoCamera(){
    p_worldCamera->toggleAutoCamera();
}

//Physics functions
void Mediator::physics_changeSimSpeed(int direction, bool pause){
    p_physicsEngine->changeSimSpeed(direction, pause);
}
WorldStats& Mediator::physics_getWorldStats(){
    return p_physicsEngine->getWorldStats();
}
void Mediator::physics_loadCollisionMeshes(std::vector<std::shared_ptr<CollisionRenderObj>>* collisionObjects){
    p_physicsEngine->loadCollisionMeshes(collisionObjects);
}
void Mediator::physics_reset(){
    p_physicsEngine->reset();
}

void Mediator::physics_addImpulseToLanderQueue(float duration, float x, float y, float z, bool torque){
    LanderObj* lo = dynamic_cast<LanderObj*>(&scene_getFocusableObject("Lander"));
    lo->addImpulseToLanderQueue(duration, x, y, z, torque);
    //p_physicsEngine->addImpulseToLanderQueue(duration, x, y, z, torque);
}
void Mediator::physics_moveLandingSite(float x, float y, float z, bool torque){
    scene_getLandingSiteObject()->moveLandingSite(x,y,z,torque); //taking a shortcut here
    //p_physicsEngine->moveLandingSite(x,y,z,torque);
}
void Mediator::physics_updateDeltaTime(){
    p_physicsEngine->updateDeltaTime();
}

//Renderer functions
Vk::RenderStats& Mediator::renderer_getRenderStats(){
    return p_renderEngine->getRenderStats();
}
std::vector<Vertex>& Mediator::renderer_getAllVertices(){
    return p_renderEngine->get_allVertices();
} //reference all loaded model vertices
std::vector<uint32_t>& Mediator::renderer_getAllIndices(){
    return p_renderEngine->get_allIndices();
} //reference all loaded model vertices
Material* Mediator::renderer_getMaterial(const std::string& name){
    return p_renderEngine->getMaterial(name);
}
void Mediator::renderer_loadTextures(const std::vector<TextureInfo>& TEXTURE_INFOS, const std::vector<std::string>& SKYBOX_PATHS){
    p_renderEngine->createTextureImages(TEXTURE_INFOS, SKYBOX_PATHS);
}
void Mediator::renderer_loadModels(const std::vector<ModelInfo>& MODEL_INFOS){
    p_renderEngine->loadModels(MODEL_INFOS);
}
void Mediator::renderer_setRenderablesPointer(std::vector<std::shared_ptr<RenderObject>>* renderableObjects){
    p_renderEngine->setRenderablesPointer(renderableObjects);
}
void Mediator::renderer_allocateDescriptorSetForTexture(const std::string& materialName, const std::string& name){
    p_renderEngine->allocateDescriptorSetForTexture(materialName, name);
}
void Mediator::renderer_allocateDescriptorSetForSkybox(){
    p_renderEngine->allocateDescriptorSetForSkybox();
}
void Mediator::renderer_allocateShadowMapImages(){
 //   p_renderEngine->allocateShadowMapImages();
}
void Mediator::renderer_setLightPointers(WorldLightObject* sceneLight, std::vector<WorldPointLightObject>* pointLights, std::vector<WorldSpotLightObject>* spotLights){
    p_renderEngine->setLightPointers(sceneLight, pointLights, spotLights);
}
void Mediator::renderer_setCameraDataPointer(CameraData* cameraData){
    p_renderEngine->setCameraData(cameraData);
}
Mesh* Mediator::renderer_getLoadedMesh(std::string name){
    return p_renderEngine->getLoadedMesh(name);
}
void Mediator::renderer_mapMaterialDataToGPU(){
    p_renderEngine->mapMaterialDataToGPU();
}
void Mediator::renderer_resetScene(){
    p_renderEngine->resetScene();
}
void Mediator::renderer_flushTextures(){
    p_renderEngine->flushTextures();
}
void Mediator::renderer_setShouldDrawOffscreen(bool b){
    p_renderEngine->setShouldDrawOffscreen(b);
}
void Mediator::renderer_writeOffscreenImageToDisk(){
    p_renderEngine->writeOffscreenImageToDisk();
}

//application functions
void Mediator::application_loadScene(SceneData sceneData){
    p_application->loadScene(sceneData);
}
void Mediator::application_endScene(){
    p_application->endScene();
}
void Mediator::application_resetScene(){
    p_application->resetScene();
}
bool Mediator::application_getSceneLoaded(){
    return p_application->getSceneLoaded();
}

//set pointers
void Mediator::setUiHandler(UiHandler* uiHandler){
    p_uiHandler = uiHandler;
}
void Mediator::setPhysicsEngine(WorldPhysics* physicsEngine){
    p_physicsEngine = physicsEngine;
}
void Mediator::setCamera(WorldCamera* camera){
    p_worldCamera = camera;
}
void Mediator::setRenderEngine(Vk::OffscreenRenderer* renderer){
    p_renderEngine = renderer;
}
void Mediator::setScene(IScene* scene){
    p_scene = scene;
}
void Mediator::setApplication(Application* application){
    p_application = application;
}