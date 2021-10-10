#pragma once
#include "vk_renderer_base.h"
#include <string>
#include "obj_render.h"
#include "dmn_iScene.h"
#include <memory>

class UiHandler; //forward declare
class WorldPhysics;
class CameraData;
class Mediator;

namespace Vk{  

class Renderer : public RendererBase{
public:
    //returns reference to allVertices, used by physics engine to copy model rather than reread obj file
    std::vector<Vertex>& get_allVertices();
    std::vector<uint32_t>& get_allIndices();

    void createTextureImages(const std::vector<TextureInfo>& TEXTURE_INFOS, const std::vector<std::string>& SKYBOX_PATHS);
    void loadModels(const std::vector<ModelInfo>& MODEL_INFOS);

    CameraData* p_cameraData;
    void setCameraData(CameraData* camData);
    void setRenderablesPointer(std::vector<std::shared_ptr<RenderObject>>* renderableObjects);
    void allocateDescriptorSetForTexture(std::string materialName, std::string name);
    void allocateDescriptorSetForSkybox();
    void allocateShadowMapImages();
    //void allocateLadarImages(); //LADAR
    void setLightPointers(WorldLightObject* sceneLight, std::vector<WorldPointLightObject>* pointLights, std::vector<WorldSpotLightObject>* spotLights);
    void mapMaterialDataToGPU();
    void resetScene();

    Renderer(GLFWwindow* windowptr, Mediator& mediator);
    void drawFrame(); //draw a frame
    Mesh* getLoadedMesh(const std::string& name); 

    void init() override;

    void setShouldDrawOffscreen(bool b);

private:
    /*uint32_t LADAR_MAP_WIDTH = 512;
    uint32_t LADAR_MAP_HEIGHT = 512;
    VkImage ladarImage;
    VkImageView ladarImageView;
    VmaAllocation ladarImageAllocation;*/

    uint32_t OFFSCREEN_IMAGE_WIDTH = 1920;
    uint32_t OFFSCREEN_IMAGE_HEIGHT = 1080;

    uint32_t SHADOW_MAP_WIDTH = 512;
    uint32_t SHADOW_MAP_HEIGHT = 512;
    VkRenderPass shadowmapRenderPass;
    VkImage shadowmapImage;
    std::vector<VkImageView> shadowmapImageViews;
    VmaAllocation shadowMapImageAllocation;
    VkFramebuffer shadowmapFramebuffer;
    VkPipeline shadowmapPipeline;
    VkPipelineLayout shadowmapPipelineLayout;


    GPUMaterialData _materialParameters[MATERIALS_COUNT];
    
    bool sceneShutdownRequest = false;

    WorldLightObject* p_sceneLight;
    std::vector<WorldPointLightObject>* p_pointLights;
    std::vector<WorldSpotLightObject>* p_spotLights;
    
    void loadScene(); //loads scene 
    void calculateFrameRate();
    
    bool frameBufferResized = false; //explicitly track framebuffer resizeing, most platforms/drivers report a resize, but it's not gauranteed, so we can check
    size_t currentFrame = 0; //tracks the current frame based on MAX_FRAMES_IN_FLIGHT and acts as an index for imageAvailable and renderFinished semaphores
    uint32_t frameCounter = 0;

    uint32_t previousFrameCount = 0; //used to track framerate
    double previousFrameTime = 0; //used to track framerate

    VkImage skyboxImage;
    VkImageView skyboxImageView;
    VmaAllocation skyboxAllocation;

    void createSyncObjects();

    void createShadowMapRenderPass();
    void createShadowMapFrameBuffer();
    void createShadowMapImage();
    void createShadowMapPipeline();
    void drawShadowMaps(int curFrame);
    void allocateDescriptorSetForShadowmap();
    //void createShadowMapCommandBuffers();

    //offscreen renderpass setup, used to draw lander optic images
    void createOffscreenRenderPass();
    void createOffscreenImageAndView();
    void createOffscreenFramebuffer();
    void createOffscreenCommandBuffer();
    void drawOffscreen(int curFrame);
    void drawOffscreenFrame();
    void createOffscreenImageBuffer();
    void fillOffscreenImageBuffer();
    //void dumpOffscreenBuffer();
    void writeOffscreenImageToDisk(); 
    VkFramebuffer offscreenFramebuffer;
    VkRenderPass offscreenRenderPass;
    VkImage offscreenImage;
    VkImageView offscreenImageView;

    VkImage offscreenImageB;
    VkImageView offscreenImageViewB;
    VmaAllocation offscreenImageAllocationB;

    VmaAllocation offscreenImageAllocation;
    VkCommandBuffer offscreenCommandBuffer;
    VkCommandPool offscreenCommandPool;
    VkBuffer offscreenImageBuffer;
    VmaAllocation offscreenImageBufferAlloc;
    bool shouldDrawOffscreenFrame = false;
    
    //Render Loop
    void drawObjects(int currentFrame);
    void recordCommandBuffer_Offscreen();
    void recordCommandBuffer_ShadowMaps(int imageIndex);
    void recordCommandBuffer_Objects(int imageIndex);
    void recordCommandBuffer_GUI(int imageIndex);
    void mapLightingDataToGPU();
    void updateLightingData(GPUCameraData& camData);
    void updateSceneData(GPUCameraData& camData);
    void populateCameraData(GPUCameraData& camData);
    void populateLanderCameraData(GPUCameraData& camData);
    void updateObjectTranslations();

    std::unordered_map<std::string,Mesh> _meshes;
    std::vector<Mesh> _loadedMeshes;

    void populateVerticesIndices(std::string path, std::unordered_map<Vertex, uint32_t>& uniqueVertices, glm::vec3 baseColour);

    GPUPointLightData _pointLightParameters[MAX_LIGHTS]; //GPU light data array thats uploaded via uniform buffer for shader referencing via push constant
    GPUSpotLightData _spotLightParameters[MAX_LIGHTS];

    GPULightVPData lightVPParameters[MAX_LIGHTS]; //GPU view projection array for rendering scene from lights perspective for generating shadowmaps

    std::vector<std::shared_ptr<RenderObject>>* p_renderables = NULL;
    UploadContext _uploadContext; //could be in public
};
}