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
    void writeOffscreenImageToDisk(); 

private:
    uint32_t OFFSCREEN_IMAGE_WIDTH = 1920;//1920;
    uint32_t OFFSCREEN_IMAGE_HEIGHT = 1080;//1080;
    const float OFFSCREEN_IMAGE_FOV = 45.0f; //degrees

    //offscreen renderpass setup, used to draw lander optic images
    void createOffscreenRenderPass();
    void createOffscreenImageAndView();
    void createOffscreenFramebuffer();
    void createOffscreenCommandBuffer();
    void drawOffscreen(int curFrame);

    //void createOffscreenImageBuffer();
    //void fillOffscreenImageBuffer();
    //VkBuffer offscreenImageBuffer;
    //VmaAllocation offscreenImageBufferAlloc;

    VkFramebuffer offscreenFramebuffer;
    VkRenderPass offscreenRenderPass;
    VkImage offscreenImage;
    VkImageView offscreenImageView;

    //VkImage offscreenImageB;
    //VkImageView offscreenImageViewB;
    //VmaAllocation offscreenImageAllocationB;

    VmaAllocation offscreenImageAllocation;
    VkCommandBuffer offscreenCommandBuffer;
    VkCommandPool offscreenCommandPool;
    
    bool shouldDrawOffscreenFrame = false;
    
    void recordCommandBuffer_Offscreen();

    void populateLanderCameraData(GPUCameraData& camData);


    };
}
