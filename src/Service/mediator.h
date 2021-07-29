#pragma once
#include "world_state.h"
#include "vk_renderer.h"

class WorldCamera;
class UiHandler;

class Mediator{
    private:
        WorldCamera* p_worldCamera;
        UiHandler* p_uiHandler;
        WorldState* p_physicsEngine;
        Vk::Renderer* p_renderEngine;
        
    public:
        //set pointers
        void setCamera(WorldCamera* camera);
        void setUiHandler(UiHandler* uiHandler);
        void setPhysicsEngine(WorldState* physicsEngine);
        void setRenderEngine(Vk::Renderer* renderer);

        //physics functions
        void physics_changeSimSpeed(int direction, bool pause);
        WorldState::WorldStats& physics_getWorldStats();
        WorldObject& physics_getWorldObject(int index);
        int physics_getWorldObjectsCount();

        //camera functions
        void camera_calculatePitchYaw(double xpos, double ypos);
        void camera_calculateZoom(double yoffset);
        void camera_changeFocus();
        void camera_setMouseLookActive(bool state);
        CameraData* camera_getCameraDataPointer();

        //renderer functions
        Vk::Renderer::RenderStats& renderer_getRenderStats();

        //ui functions
        void ui_toggleEscMenu();
};
