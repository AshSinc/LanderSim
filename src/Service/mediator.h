#pragma once
#include "world_physics.h"
#include "vk_renderer.h"
#include "obj.h"
#include <string>
#include "dmn_iScene.h"

class WorldCamera;
class UiHandler;

class Mediator{
    private:
        WorldCamera* p_worldCamera;
        UiHandler* p_uiHandler;
        WorldPhysics* p_physicsEngine;
        Vk::Renderer* p_renderEngine;
        IScene* p_scene;
        
    public:
        //set pointers
        void setCamera(WorldCamera* camera);
        void setUiHandler(UiHandler* uiHandler);
        void setPhysicsEngine(WorldPhysics* physicsEngine);
        void setRenderEngine(Vk::Renderer* renderer);

        //physics functions
        void physics_changeSimSpeed(int direction, bool pause);
        WorldPhysics::WorldStats& physics_getWorldStats();
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
        int renderer_getMeshId(const std::string& name);
        Material* renderer_getMaterial(const std::string& name);
        void renderer_loadTextures(const std::vector<TextureInfo>& TEXTURE_INFOS, const std::vector<std::string>& SKYBOX_PATHS);
        void renderer_loadModels(const std::vector<ModelInfo>& MODEL_INFOS);
        void renderer_setRenderablesPointer(std::vector<RenderObject&>* renderableObjects);
        void renderer_allocateDescriptorSetForTexture(const std::string& materialName, const std::string& name);
        void renderer_allocateDescriptorSetForSkybox();
        void renderer_setLightPointers(WorldLightObject* sceneLight, std::vector<WorldPointLightObject>* pointLights, std::vector<WorldSpotLightObject>* spotLights);

        //Scene functions
        //int scene_getNumPointLights();
        //int scene_getNumSpotLights();

        //ui functions
        void ui_toggleEscMenu();
        void ui_updateUIPanelDimensions(GLFWwindow* window);
        void ui_drawUI();
};
