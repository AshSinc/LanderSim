#pragma once
#include "vk_renderer.h"
#include "opencv2/opencv.hpp"
#include <array>

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
    
    std::vector<ImguiTexturePacket>& getDstTexturePackets();

    std::deque<int> getImguiTextureSetIndicesQueue(){return imguiTextureSetIndicesQueue;};

    std::deque<int> getImguiDetectionIndicesQueue(){return imguiDetectionIndicesQueue;};

    std::deque<int> getImguiMatchIndicesQueue(){return imguiMatchIndicesQueue;};

    void popCvMatQueue();
    cv::Mat& frontCvMatQueue();

    bool cvMatQueueEmpty(){return cvMatQueue.empty();};

    void assignMatToDetectionView(cv::Mat image);
    void assignMatToMatchingView(cv::Mat image);
    void clearOpticsViews();
    void setOpticsFov(float fov){OFFSCREEN_IMAGE_FOV = fov;};

private:

    glm::vec4 LANDER_OPTICS_AMBIENT = glm::vec4(0.005f, 0.005f, 0.005f, 1);
    glm::vec4 LANDER_OPTICS_DIFFUSE = glm::vec4(0.75f, 0.75f, 0.75f, 1);
    glm::vec4 LANDER_OPTICS_SPECULAR = glm::vec4(5.0f,5.0f,5.0f, 1);

    int NUM_TEXTURE_SETS = 2; //4 sets
    int NUM_TEXTURES_IN_SET = 2; //2 images in each set

    std::vector<ImguiTexturePacket> imguiTexturePackets;
    
    std::deque<int> imguiTextureSetIndicesQueue;

    std::deque<int> imguiDetectionIndicesQueue;

    std::deque<int> imguiMatchIndicesQueue;

    std::vector<Texture> opticsTextures;

    std::vector<Texture> detectionTextures;

    Texture matchTexture;

    std::deque<cv::Mat> cvMatQueue;

    int opticsFrameCounter = NUM_TEXTURE_SETS-1; //start at max, instantly go to zero at start of copying, allows syncing

    void convertOffscreenImage();

    //too much work to rewrite render pipeline and everything that goes with it to output lower resolution natively, too messy, need to start from scratch but not in this project
    //instead we render in the full window size (need to check window size this is set statically for now)
    //then copy the image to a smaller VkImage, copy was needed anyway
    uint32_t RENDERED_IMAGE_WIDTH = 1920; //this is full rendered resolution on GPU RAM
    uint32_t RENDERED_IMAGE_HEIGHT = 1080; 
    uint32_t OUTPUT_IMAGE_WH = 512; //this is desired output resolution, width and height 
    uint32_t OFFSCREEN_IMAGE_WIDTH_OFFSET = (RENDERED_IMAGE_WIDTH/2) - (OUTPUT_IMAGE_WH/2); //this works out offset to get center of image
    uint32_t OFFSCREEN_IMAGE_HEIGHT_OFFSET = (RENDERED_IMAGE_HEIGHT/2) - (OUTPUT_IMAGE_WH/2);
    float OFFSCREEN_IMAGE_FOV = 5.0f; //degrees

    const char* dstImageMappedData;
    std::vector<const char*> detectionImageMappings;
    const char* matchImageMapping;

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
    void createSamplers();

    void updateSceneData(GPUCameraData& camData); //temp overridden for testing optical settings

    VkFence offscreenCopyFence;

    void createOffscreenImageBuffer();
    void mapLightingDataToGPU();
    void updateLightingData(GPUCameraData& camData);

    VkBuffer offscreenImageBuffer;
    VmaAllocation offscreenImageBufferAlloc;

    std::mutex copyLock;
    //std::mutex copyLock;

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

    VkImage cropImage;
    VmaAllocation cropImageAllocation;

    VkImage greyRGBImage;
    VmaAllocation greyRGBImageAllocation;
    VkImageView greyRGBImageView;
    VkSampler greyRGBImageSampler;

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
