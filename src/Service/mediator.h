#pragma once
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <glm/vec3.hpp>

#include <imgui_impl_vulkan.h> //and vulkan

#include <array>
#include <deque>

#include "opencv2/opencv.hpp"

#include "filewriter.h"

class WorldCamera;
class UiHandler;
class Application;
class WorldPhysics;
class RenderObject;
class CollisionRenderObj;

class GLFWwindow;

namespace Vk{
    class OffscreenRenderer;
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
struct LandingSiteObj;
struct LanderObj;
struct LanderBoostCommand;

struct ImguiTexturePacket;

class IScene;

class Mediator{
    private:
        WorldCamera* p_worldCamera;
        UiHandler* p_uiHandler;
        WorldPhysics* p_physicsEngine;
        Vk::OffscreenRenderer* p_renderEngine;
        IScene* p_scene;
        Application* p_application;
        Service::Writer* p_writer;
        
    public:
        //set pointers
        void setCamera(WorldCamera* camera);
        void setUiHandler(UiHandler* uiHandler);
        void setPhysicsEngine(WorldPhysics* physicsEngine);
        void setRenderEngine(Vk::OffscreenRenderer* renderer);
        void setScene(IScene* scene);
        void setApplication(Application* application);
        void setWriter(Service::Writer* writer);

        //writer functions
        void writer_writeToFile(std::string file, std::string text);

        //physics functions
        void physics_changeSimSpeed(int direction, bool pause);
        WorldStats& physics_getWorldStats();
        void physics_loadCollisionMeshes(std::vector<std::shared_ptr<CollisionRenderObj>>* collisionObjects);
        void physics_reset();
        //bool physics_landerImpulseRequested();
        //LanderBoostCommand& physics_popLanderImpulseQueue();
        void physics_addImpulseToLanderQueue(float duration, float x, float y, float z, bool torque = false);
        void physics_moveLandingSite(float x, float y, float z, bool torque = false);
        void physics_updateDeltaTime();
        void physics_landerCollided();
        //void physics_initDynamicsWorld();
        glm::vec3 physics_performRayCast(glm::vec3 from, glm::vec3 dir, float range);
        double physics_getTimeStamp();

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
        void renderer_setShouldDrawOffscreen(bool b);
        std::vector<ImguiTexturePacket>& renderer_getDstTexturePackets();
        std::deque<int> renderer_getImguiTextureSetIndicesQueue();
        std::deque<int> renderer_getImguiDetectionIndicesQueue();
        std::deque<int> renderer_getImguiMatchIndicesQueue();
        void renderer_popCvMatQueue();
        cv::Mat& renderer_frontCvMatQueue();
        bool renderer_cvMatQueueEmpty();
        void renderer_assignMatToDetectionView(cv::Mat image);
        void renderer_assignMatToMatchingView(cv::Mat image);
        void renderer_clearOpticsViews();
        void renderer_setOpticsFov(float fov);
    
        //Scene functions
        int scene_getWorldObjectsCount();
        WorldObject& scene_getWorldObject(int i);
        WorldObject& scene_getFocusableObject(std::string name);
        LandingSiteObj* scene_getLandingSiteObject();
        LanderObj* scene_getLanderObject();
        std::vector<std::shared_ptr<RenderObject>>* scene_getDebugObjects();

        //ui functions
        void ui_toggleEscMenu();
        void ui_updateUIPanelDimensions(GLFWwindow* window);
        void ui_drawUI();
        void ui_updateLoadingProgress(float progress, std::string text);
        void ui_submitBoostCommand(LanderBoostCommand boost);

        //application functions
        void application_loadScene(SceneData sceneData);
        void application_endScene();
        void application_resetScene();
        bool application_getSceneLoaded();
};
