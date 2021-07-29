#include "mediator.h"
#include "world_camera.h"
#include "ui_handler.h"

//Ui functions
void Mediator::ui_toggleEscMenu(){
    p_uiHandler->toggleMenu();
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
WorldState::WorldStats& Mediator::physics_getWorldStats(){
    return p_physicsEngine->getWorldStats();
}
WorldObject& Mediator::physics_getWorldObject(int index){
    return p_physicsEngine->getWorldObject(index);
}
int Mediator::physics_getWorldObjectsCount(){
    return p_physicsEngine->getWorldObjectsCount();
}

//Renderer functions
Vk::Renderer::RenderStats& Mediator::renderer_getRenderStats(){
    return p_renderEngine->getRenderStats();
}

//set pointers
void Mediator::setUiHandler(UiHandler* uiHandler){
    p_uiHandler = uiHandler;
}
void Mediator::setPhysicsEngine(WorldState* physicsEngine){
    p_physicsEngine = physicsEngine;
}
void Mediator::setCamera(WorldCamera* camera){
    p_worldCamera = camera;
}
void Mediator::setRenderEngine(Vk::Renderer* renderer){
    p_renderEngine = renderer;
}