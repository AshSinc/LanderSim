#include "mediator.h"
#include "world_camera.h"
#include "ui_handler.h"


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

//Physics functions
void Mediator::physics_changeSimSpeed(int direction, bool pause){
    p_physicsEngine->changeSimSpeed(direction, pause);
}
WorldPhysics::WorldStats& Mediator::physics_getWorldStats(){
    return p_physicsEngine->getWorldStats();
}
//WorldObject& Mediator::physics_getWorldObject(int index){
//    return p_physicsEngine->getWorldObject(index);
//}
int Mediator::physics_getWorldObjectsCount(){
    return p_physicsEngine->getWorldObjectsCount();
}

//Renderer functions
Vk::Renderer::RenderStats& Mediator::renderer_getRenderStats(){
    return p_renderEngine->getRenderStats();
}
int Mediator::renderer_getMeshId(const std::string& name){
    return p_renderEngine->getMeshId(name);
}
Material* Mediator::renderer_getMaterial(const std::string& name){
    return p_renderEngine->get_material(name);
}
void Mediator::renderer_loadTextures(const std::vector<TextureInfo>& TEXTURE_INFOS, const std::vector<std::string>& SKYBOX_PATHS){
    p_renderEngine->createTextureImages(TEXTURE_INFOS, SKYBOX_PATHS);
}
void Mediator::renderer_loadModels(const std::vector<ModelInfo>& MODEL_INFOS){
    p_renderEngine->loadModels(MODEL_INFOS);
}
void Mediator::renderer_setRenderablesPointer(std::vector<RenderObject&>* renderableObjects){
    p_renderEngine->setRenderablesPointer(renderableObjects);
}
void Mediator::renderer_allocateDescriptorSetForTexture(const std::string& materialName, const std::string& name){
    p_renderEngine->allocateDescriptorSetForTexture(materialName, name);
}
void Mediator::renderer_allocateDescriptorSetForSkybox(){
    p_renderEngine->allocateDescriptorSetForSkybox();
}
void Mediator::renderer_setLightPointers(WorldLightObject* sceneLight, std::vector<WorldPointLightObject>* pointLights, std::vector<WorldSpotLightObject>* spotLights){
    p_renderEngine->setLightPointers(sceneLight, pointLights, spotLights);
}
//Scene functions
/*int Mediator::scene_getNumPointLights(){
    p_scene->getNumPointLights();
}
int Mediator::scene_getNumSpotLights(){
    p_scene->getNumSpotLights();
}*/

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
void Mediator::setRenderEngine(Vk::Renderer* renderer){
    p_renderEngine = renderer;
}