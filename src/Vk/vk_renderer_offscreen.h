#pragma once
#include "vk_renderer.h"
//#include <glfw/vulkan>
class GLFWwindow;
class Mediator;

namespace Vk{
class OffscreenRenderer : public Renderer{
public:
    OffscreenRenderer(GLFWwindow* windowptr, Mediator& mediator);
    void init() override;
    void drawFrame(); //draw a frame
    void setShouldDrawOffscreen(bool b);
    void cleanup();
    
private:

    void convertOffscreenImage();

    //too much work to rewrite render pipeline and everything that goes with it to output lower resolution natively, too messy, need to start from scratch but not in this project
    //instead we render in the full window size (need to check window size this is set statically for now)
    //then copy the image to a smaller VkImage, copy was needed anyway
    uint32_t RENDERED_IMAGE_WIDTH = 1920; //this is full rendered resolution on GPU RAM
    uint32_t RENDERED_IMAGE_HEIGHT = 1080; 
    uint32_t OUTPUT_IMAGE_WH = 512; //this is desired output resolution, width and height 
    uint32_t OFFSCREEN_IMAGE_WIDTH_OFFSET = (RENDERED_IMAGE_WIDTH/2) - (OUTPUT_IMAGE_WH/2); //this works out offset to get center of image
    uint32_t OFFSCREEN_IMAGE_HEIGHT_OFFSET = (RENDERED_IMAGE_HEIGHT/2) - (OUTPUT_IMAGE_WH/2);
    const float OFFSCREEN_IMAGE_FOV = 45.0f; //degrees

    const char* mappedData;
    //uint32_t rowPitch;

    //offscreen renderpass setup, used to draw lander optic images
    void createOffscreenRenderPass();
    void createOffscreenImageAndView();
    void createOffscreenFramebuffer();
    void createOffscreenCommandBuffer();
    void drawOffscreen(int curFrame);
    void createOffscreenColourResources();
    void createOffscreenDepthResources();
    void createOffscreenCameraBuffer();
    void createOffscreenDescriptorSet();
    void createOffscreenSyncObjects();

    void updateSceneData(GPUCameraData& camData); //temp overridden for testing optical settings

    //VkFence offscreenRenderFence;
    VkFence offscreenCopyFence;

    void createOffscreenImageBuffer();
    void mapLightingDataToGPU();
    void updateLightingData(GPUCameraData& camData);
    //void fillOffscreenImageBuffer();
    VkBuffer offscreenImageBuffer;
    VmaAllocation offscreenImageBufferAlloc;

    std::mutex copyLock;

    VkFramebuffer offscreenFramebuffer;
    VkRenderPass offscreenRenderPass;
    VkImage offscreenImage;
    VkImageView offscreenImageView;

    VkImage offscreenColourImage;
    VkImageView offscreenColourImageView;
    VmaAllocation offscreenColourImageAllocation;

    VkImage offscreenDepthImage;
    VkImageView offscreenDepthImageView;
    VmaAllocation offscreenDepthImageAllocation;

    VkImage dstImage;
    VmaAllocation dstImageAllocation;
    VkImage greyImage;
    VmaAllocation greyImageAllocation;

    VmaAllocation offscreenImageAllocation;
    VkCommandBuffer offscreenCommandBuffer;
    VkCommandPool offscreenCommandPool;

    VkBuffer offscreenCameraBuffer;
    VmaAllocation offscreenCameraBufferAllocation;
    VkBuffer offscreenSceneParameterBuffer;
    VmaAllocation offscreenSceneParameterBufferAlloc;

    VkBuffer os_spotLightParameterBuffer;
    VmaAllocation os_spotLightParameterBufferAlloc;

    VkBuffer os_objectBuffer;
    VmaAllocation os_objectBufferAlloc;

    //GPUSceneData os_sceneParameters;
    //GPUPointLightData os_pointLightParameters[MAX_LIGHTS]; //GPU light data array thats uploaded via uniform buffer for shader referencing via push constant
    //GPUSpotLightData os_spotLightParameters[MAX_LIGHTS];
    //GPULightVPData os_lightVPParameters[MAX_LIGHTS]; //GPU view projection array for rendering scene from lights perspective for generating shadowmaps


    VkDescriptorSet offscreenDescriptorSet;
    VkDescriptorSetLayout offscreenGlobalSetLayout;

    VkDescriptorSet os_objectDescriptor;
    VkDescriptorSetLayout os_objectSetLayout;

    VkDescriptorSet os_lightSet;
    VkDescriptorSetLayout os_lightSetLayout;
    
    bool shouldDrawOffscreenFrame = false;
    bool renderSubmitted = false;
    
    void recordCommandBuffer_Offscreen();

    void populateLanderCameraData(GPUCameraData& camData);
   
    
    };
}
