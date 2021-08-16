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

const int MAX_OBJECTS = 1000; //used to set max object buffer size, could probably be 100k or higher easily but no need for now
const int MAX_FRAMES_IN_FLIGHT = 2; //maximum concurrent frames in pipeline, i think 2 is standard according to this study by intel https://software.intel.com/content/www/us/en/develop/articles/practical-approach-to-vulkan-part-1.html
const int MATERIALS_COUNT = 4; // set the count of materials, for sizing the _materialParameters array, needs to be adjusted in shaders manually
const int MAX_LIGHTS = 10;

class Renderer : public RendererBase{
public:
    //returns reference to allVertices, used by physics engine to copy model rather than reread obj file
    std::vector<Vertex>& get_allVertices();
    std::vector<uint32_t>& get_allIndices();

    Mesh* get_mesh(const std::string& name);
    int getMeshId(const std::string& name);

    void createTextureImages(const std::vector<TextureInfo>& TEXTURE_INFOS, const std::vector<std::string>& SKYBOX_PATHS);
    void loadModels(const std::vector<ModelInfo>& MODEL_INFOS);

    CameraData* p_cameraData;
    void setCameraData(CameraData* camData);
    void setRenderablesPointer(std::vector<std::shared_ptr<RenderObject>>* renderableObjects);
    void allocateDescriptorSetForTexture(std::string materialName, std::string name);
    void allocateDescriptorSetForSkybox();
    void setLightPointers(WorldLightObject* sceneLight, std::vector<WorldPointLightObject>* pointLights, std::vector<WorldSpotLightObject>* spotLights);
    Mesh* getLoadedMesh(std::string name);
    void mapMaterialDataToGPU();
    void reset();
    void callReset();

    Renderer(GLFWwindow* windowptr, Mediator& mediator);
    void drawFrame(); //draw a frame
private:
    GPUMaterialData _materialParameters[MATERIALS_COUNT];
    
    bool resetTextureImages = false;

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
    
    //Render Loop
    void drawObjects(int currentFrame);
    void rerecordCommandBuffer(int i);
    void mapLightingDataToGPU();
    void updateLightingData(GPUCameraData& camData);
    void updateSceneData(GPUCameraData& camData);
    
    
    void populateCameraData(GPUCameraData& camData);
    void updateObjectTranslations();

    std::unordered_map<std::string,Mesh> _meshes;
    std::vector<Mesh> _loadedMeshes;

    void populateVerticesIndices(std::string path, std::unordered_map<Vertex, uint32_t>& uniqueVertices, glm::vec3 baseColour);

    GPUPointLightData _pointLightParameters[MAX_LIGHTS]; //GPU material data array thats uploaded via uniform buffer for shader referencing via push constant
    GPUSpotLightData _spotLightParameters[MAX_LIGHTS]; //GPU material data array thats uploaded via uniform buffer for shader referencing via push constant

    std::vector<std::shared_ptr<RenderObject>>* p_renderables = NULL;
    UploadContext _uploadContext; //could be in public
};
}
//#endif /* !VK_ENGINE_D */