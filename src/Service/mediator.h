#pragma once
#include <string>
#include <map>
#include <vector>
#include <memory>

class WorldCamera;
class UiHandler;
class Application;
class WorldPhysics;
class CollisionRenderObj;
class RenderObject;
class GLFWwindow;

namespace Vk{
    class Renderer;
    struct RenderStats;
}

struct CameraData;
struct Mesh;
struct Vertex;
struct Material;
struct TextureInfo;
struct ModelInfo;
struct WorldObject;
struct WorldLightObject;
struct WorldPointLightObject;
struct WorldSpotLightObject;
struct SceneData;
struct WorldStats;

class IScene;

class Mediator{
    private:
        WorldCamera* p_worldCamera;
        UiHandler* p_uiHandler;
        WorldPhysics* p_physicsEngine;
        Vk::Renderer* p_renderEngine;
        IScene* p_scene;
        Application* p_application;
        
    public:
        //set pointers
        void setCamera(WorldCamera* camera);
        void setUiHandler(UiHandler* uiHandler);
        void setPhysicsEngine(WorldPhysics* physicsEngine);
        void setRenderEngine(Vk::Renderer* renderer);
        void setScene(IScene* scene);
        void setApplication(Application* application);

        //physics functions
        void physics_changeSimSpeed(int direction, bool pause);
        WorldStats& physics_getWorldStats();
        void physics_loadCollisionMeshes(std::vector<std::shared_ptr<CollisionRenderObj>>* collisionObjects);
        void physics_reset();

        //camera functions
        void camera_calculatePitchYaw(double xpos, double ypos);
        void camera_calculateZoom(double yoffset);
        void camera_changeFocus();
        void camera_setMouseLookActive(bool state);
        CameraData* camera_getCameraDataPointer();
        void camera_toggleAutoCamera();

        //renderer functions
        Vk::RenderStats& renderer_getRenderStats();
        std::vector<Vertex>& renderer_getAllVertices();
        std::vector<uint32_t>& renderer_getAllIndices();
        Material* renderer_getMaterial(const std::string& name);
        void renderer_loadTextures(const std::vector<TextureInfo>& TEXTURE_INFOS, const std::vector<std::string>& SKYBOX_PATHS);
        void renderer_loadModels(const std::vector<ModelInfo>& MODEL_INFOS);
        void renderer_setRenderablesPointer(std::vector<std::shared_ptr<RenderObject>>* renderableObjects);
        void renderer_allocateDescriptorSetForTexture(const std::string& materialName, const std::string& name);
        void renderer_allocateDescriptorSetForSkybox();
        void renderer_allocateShadowMapImages();
        void renderer_setLightPointers(WorldLightObject* sceneLight, std::vector<WorldPointLightObject>* pointLights, std::vector<WorldSpotLightObject>* spotLights);
        void renderer_setCameraDataPointer(CameraData* cameraData);
        Mesh* renderer_getLoadedMesh(std::string name);
        void renderer_mapMaterialDataToGPU();
        void renderer_resetScene();
        void renderer_flushTextures();

        //Scene functions
        int scene_getWorldObjectsCount();
        WorldObject& scene_getWorldObject(int i);
        WorldObject& scene_getFocusableObject(std::string name);

        //ui functions
        void ui_toggleEscMenu();
        void ui_updateUIPanelDimensions(GLFWwindow* window);
        void ui_drawUI();
        void ui_updateLoadingProgress(float progress, std::string text);

        //application functions
        void application_loadScene(SceneData sceneData);
        void application_endScene();
        void application_resetScene();
        bool application_getSceneLoaded();
};
