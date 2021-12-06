#pragma once
#include <deque>
#include <functional> //these are used for deletion queues
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "vk_debug_msg.h" //our seperated debug messenger, just to get some of the code out
#include "vk_windowHandler.h" //our window manager, just to get some of the code out
#include "vk_types.h"
#include <mutex>
#include "ui_handler.h"
#include "vk_mesh.h"
#include <unordered_map>
#include <vector>
#include <vk_mem_alloc.h>

class UiHandler; //forward declare
class WorldPhysics;
class CameraData;
class Mediator;
class GLFWwindow;
class Mediator;
class WindowHandler;
class Material;
class Vertex;

namespace Vk{
    class ImageHelper;

    const int MAX_OBJECTS = 1000; //used to set max object buffer size, could probably be 100k or higher easily but no need for now
    const int MAX_FRAMES_IN_FLIGHT = 2; //maximum concurrent frames in pipeline, i think 2 is standard according to this study by intel https://software.intel.com/content/www/us/en/develop/articles/practical-approach-to-vulkan-part-1.html
    const int MATERIALS_COUNT = 5; // set the count of materials, for sizing the _materialParameters array, needs to be adjusted in shaders manually
    const int MAX_LIGHTS = 10;

    struct RenderStats{
        double framerate = 0;
        double fps = 0;
    };

    class RendererBase{
    public:

        RenderStats renderStats;
        RenderStats& getRenderStats();

        struct DeletionQueue{
            std::deque<std::function<void()>> deletors;
            void push_function(std::function<void()>&& function){
                deletors.push_back(function);
            }
            void flush() {
                // reverse iterate the deletion queue to execute all the functions
                for (auto it = deletors.rbegin(); it != deletors.rend(); it++){
                    (*it)(); //call functors
                }
                deletors.clear();
            }
        };

        DeletionQueue _mainDeletionQueue;
        DeletionQueue _textureDeletionQueue;
        DeletionQueue _swapDeletionQueue;
        DeletionQueue _modelBufferDeletionQueue;

        //constructor, should have code to enforce one instance
        RendererBase(GLFWwindow* windowptr, Mediator& mediator);
        //Exposed Functions for Main.cpp
        virtual void init(); //initialise engine
        virtual void drawFrame() = 0; //draw a frame
        void cleanup(); //cleanup objects

        void flushTextures();
        void flushSceneBuffers();

        //Expose the allocator and CreateBuffer and createImage functions
        VmaAllocator allocator; //VMA allocator for handling resource allocation (only used for vertex and indicex buffers just now, can be used for images too)
        void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage vmaUsage, VkBuffer& buffer, VmaAllocation& allocation);

        //these need moved to seperate file?
        VkCommandBuffer beginSingleTimeCommands(VkCommandPool pool);
        void endSingleTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool pool, VkQueue queue);

        //exposes initialization variables for use in utility namespaces (vk_images)
        VkDevice device; //handle to the logical device
        VkPhysicalDevice physicalDevice;// = VK_NULL_HANDLE; //stores the physical device handle
        VkCommandPool transferCommandPool; //stores our transient command pool (for short lived command buffers) //TESTING
        VkQueue graphicsQueue; //handle to the graphics queue, queues are automatically created with the logical device
        QueueFamilyIndices queueFamilyIndicesStruct;// store the struct so we only call findQueueFamilies once
        Material* getMaterial(const std::string& name);
        private:        
        void createVkInstance();
        void createCommandPools();

        void flushSwapChain();
 
        //#ifdef NDEBUG //NDEBUG is a standard C++ macro for checking if we are compiling in debug mode or not
        //const bool enableValidationLayers = false;
        //#else
        const bool enableValidationLayers = true; //if we are we want to use Validation layers to be able to define error checking
        //#endif
        //required extensions and debug validation layers
        const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME}; //list of required extensions (macro expands to VK_KHR_swapchain)
        const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"}; //list of required validation layers, for debugging

        void pickPhysicalDevice();
        void createLogicalDevice();
        void createMemAlloc();
        void createSwapChain();
        void createSwapChainImageViews();
        void createDescriptorSetLayouts();

        UploadContext _uploadContext; //could be in public

        void createColourResources();
        void createDepthResources();

        void createDescriptorPool();
        void createDescriptorSets();
        void createUniformBuffers();
        void createSamplers();
        void createRenderPass();
        void createFramebuffers();
        void createCommandBuffers();
        void createSyncObjects();
        void initUI();

        VkImage depthImage; //depth image to draw to for calulcating depth
        VmaAllocation depthImageAllocation;
        //VkImageView depthImageView;
        VkImage colourImage; //colour image to draw to for calulcating final output colour
        VmaAllocation colourImageAllocation;
        //VkImageView colourImageView;
        
        std::unordered_map<std::string, Texture> _skyboxTextures;

        void initPipelines(); //load and configure pipelines and correspending shader stages and assign to materials
        
        
        VmaAllocation vertexBufferAllocation; //stores the handle to the assigned memory on the GPU 
        VmaAllocation indexBufferAllocation; //stores the handle to the assigned memory on the GPU 
        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size); //no need for public right now 
        
        protected:
        VkImageView colourImageView;
        VkImageView depthImageView;

        VkShaderModule createShaderModule(const std::vector<char>& code);
        std::unordered_map<std::string,Material> _materials;
        Material* createMaterial(VkPipeline pipeline, VkPipelineLayout layout,const std::string& name, int id);

        VkBuffer _sceneParameterBuffer;
        GPUSceneData _sceneParameters; //uniform buffer for scene variables for the shaders
        VmaAllocation _sceneParameterBufferAlloc;

        std::vector<Vertex> allVertices;
        std::vector<uint32_t> allIndices;
        VkBuffer vertexBuffer; //holds the vertex buffer handle
        VkBuffer indexBuffer; //holds the index buffer handle (for specifying vertices by index in the vertexBuffer)
        VkDescriptorSet skyboxSet;
        void createVertexBuffer(); //depends on vertex sizes and layout, might be able to overallocate?
        void createIndexBuffer(); //depends on index sizes and layout, might be able to overallocate?
        void recreateSwapChain();
        UiHandler* p_uiHandler;
        std::vector<VkClearValue> clearValues; //stores clean values for clearing an image

        VkDescriptorSetLayout _multiTextureSetLayout;
        VkDescriptorSetLayout _globalSetLayout; //handle to our global descriptor layout used for ubo, camera and sampler, set 0
        VkDescriptorSetLayout _objectSetLayout; //handle to our object descriptor layout,
        VkDescriptorSetLayout _materialSetLayout; 
        VkDescriptorSetLayout _lightingSetLayout; 
        VkDescriptorSetLayout _skyboxSetLayout;
        VkDescriptorSetLayout lightVPSetLayout; //handle to our light VP descriptor layout
        
        VkSampler skyboxSampler;
        VkDescriptorPool descriptorPool; //handle to our descriptor pool (need to read more about this)

        VkSampler diffuseSampler; //sampler
        VkSampler specularSampler; //sampler
        //VkSampler shadowmapSampler; //sampler
        VkPipelineLayout _skyboxPipelineLayout;
        std::unordered_map<std::string, Texture> _loadedTextures;

        std::vector<VkFramebuffer> swapChainFramebuffers; //holds a framebuffer foreach VkImage in the swapchain

        VkPipeline skyboxPipeline;

        VkRenderPass guiRenderPass; //handle to gui render pass
        VkCommandPool guiCommandPool;
        std::vector<VkCommandBuffer> guiCommandBuffers;
        std::vector<VkFramebuffer> guiFramebuffers; //holds a framebuffer foreach VkImage in the swapchain

        std::mutex queueSubmitMutex;
        Mediator& r_mediator;
        
        Vk::ImageHelper* imageHelper;//would be better with unique_ptr, or just regular object! why doesnt that work, think its circular includes
        VkInstance instance;
        VkDebugUtilsMessengerEXT debugMessenger; //can have as many of these debug messenger obj as we want
        Vk::Debug::Messenger extDebug; //external debug trying to remove from code
        WindowHandler windowHandler;
        GLFWwindow* window; //pointer to the window
        VkSurfaceKHR surface; //represents an abstract type of surface to present rendered images to
        VkPhysicalDeviceProperties _gpuProperties;
        VkQueue presentQueue; //handle to presentation queue for drawing to a surface
        VkQueue transferQueue; //handle to transfer queue for explicit transfers
        VkSwapchainKHR swapChain; //handle to the swapchain
        std::vector<VkImage> swapChainImages; //stores the handles of the VkImages that are in the created swap chain
        VkFormat swapChainImageFormat; //stores the format we are using in the swap chain for later use
        VkExtent2D swapChainExtent; //stores the extent we specified in the swap chain for later use
        std::vector<VkImageView> swapChainImageViews; //stores the image views
        VkRenderPass renderPass; //handle to render pass object

        VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT; //MSAA, we will use a default of 1 sample per pixel which is no MSAA, but then check for support and adjsut at runtime
        
        std::vector<FrameData> _frames; //stores per frame variables like buffers and synchronization variables

        VkCommandPool transientCommandPool; //stores our transient command pool (for short lived command buffers) //TESTING

        size_t pad_uniform_buffer_size(size_t originalSize);
        std::vector<VkFence> imagesInFlight;

        void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free);
    };
}