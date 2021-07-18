#ifndef VK_ENGINE_D
#define VK_ENGINE_D
#include <vk_mem_alloc.h> //we are using VMA (Vulkan Memory Allocator to handle allocating blocks of memory for resources)

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include "vk_types.h"
#include "glfw_WindowHandler.h"
#include "vk_debug_msg.h"
#include "vk_mesh.h"
#include <unordered_map>

class UiHandler; //forward declare
class WorldState;

//Engine Constants
const int MAX_OBJECTS = 1000; //used to set max object buffer size, could probably be 100k or higher easily but no need for now
const int MAX_FRAMES_IN_FLIGHT = 2; //maximum concurrent frames in pipeline, i think 2 is standard according to this study by intel https://software.intel.com/content/www/us/en/develop/articles/practical-approach-to-vulkan-part-1.html
const int MATERIALS_COUNT = 4; // set the count of materials, for sizing the _materialParameters array, needs to be adjusted in shaders manually
const int MAX_LIGHTS = 10;

//model identifier and path pairs, for assigning to unnordered map, loading code needs cleaned and moved
const std::vector<ModelInfo> MODEL_INFOS = {
    {"satellite", "models/Satellite.obj"},
    {"asteroid", "models/asteroid.obj"},
    //{"asteroid", "models/Bennu.obj"},
    {"sphere", "models/sphere.obj"},
    {"box", "models/box.obj"}
};

//texture identifier and path pairs, used in loading and assigning to map
const std::vector<TextureInfo> TEXTURE_INFOS = {
    {"satellite_diff", "textures/Satellite.jpg"},
    {"satellite_spec", "textures/Satellite_Specular.jpg"},
    //{"asteroid_diff", "textures/ASTEROID_COLOR.jpg"},
    //{"asteroid_spec", "textures/ASTEROID_SPECULAR.jpg"}
    {"asteroid_diff", "textures/ASTEROID_COLORB.jpg"},
    {"asteroid_spec", "textures/ASTEROID_COLORB.jpg"}
};

//texture identifier and path pairs, used in loading and assigning to map
const std::vector<std::string> SKYBOX_PATHS = {
    {"textures/skybox/GalaxyTex_PositiveX.jpg"},
    {"textures/skybox/GalaxyTex_NegativeX.jpg"},
    {"textures/skybox/GalaxyTex_PositiveZ.jpg"},
    {"textures/skybox/GalaxyTex_NegativeZ.jpg"},
    {"textures/skybox/GalaxyTex_NegativeY.jpg"},
    {"textures/skybox/GalaxyTex_PositiveY.jpg"}
};

//required extensions and debug validation layers
const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME}; //list of required extensions (macro expands to VK_KHR_swapchain)
const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"}; //list of required validation layers, for debugging
#ifdef NDEBUG //NDEBUG is a standard C++ macro for checking if we are compiling in debug mode or not
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true; //if we are we want to use Validation layers to be able to define error checking
#endif

class VulkanEngine {
public:

    struct RenderStats{
        double framerate = 0;
        double fps = 0;
    }renderStats;

    RenderStats getRenderStats();

    struct DeletionQueue{
        std::deque<std::function<void()>> deletors;
        void push_function(std::function<void()>&& function) {
            deletors.push_back(function);
        }
        void flush() {
            // reverse iterate the deletion queue to execute all the functions
            for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
                (*it)(); //call functors
            }
            deletors.clear();
        }
    };
    DeletionQueue _mainDeletionQueue;
    DeletionQueue _swapDeletionQueue;

    //constructor, should have code to enforce one instance
    VulkanEngine(GLFWwindow* windowptr, WorldState* state);
    //Exposed Functions for Main.cpp
    void init(UiHandler* uiHandler); //initialise engine
    void drawFrame(); //draw a frame
    void cleanup(); //cleanup objects

    //Expose the allocator and CreateBuffer and createImage functions
    static VmaAllocator allocator; //VMA allocator for handling resource allocation (only used for vertex and indicex buffers just now, can be used for images too)
    static void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage vmaUsage, VkBuffer& buffer, VmaAllocation& allocation);

    //these need moved to seperate file?
    static VkCommandBuffer beginSingleTimeCommands(VkCommandPool pool);
    static void endSingleTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool pool, VkQueue queue);

    //exposes initialization variables for use in utility namespaces (vk_images)
    static VkDevice device; //handle to the logical device
    static VkPhysicalDevice physicalDevice;// = VK_NULL_HANDLE; //stores the physical device handle
    static VkCommandPool transferCommandPool; //stores our transient command pool (for short lived command buffers) //TESTING
    static VkQueue graphicsQueue; //handle to the graphics queue, queues are automatically created with the logical device
    static QueueFamilyIndices queueFamilyIndicesStruct;// store the struct so we only call findQueueFamilies once

    //returns reference to allVertices, used by physics engine to copy model rather than reread obj file
    std::vector<Vertex>& get_allVertices();
    std::vector<uint32_t>& get_allIndices();

    Mesh* get_mesh(const std::string& name);
    
private:
    UiHandler* p_uiHandler;
    void loadScene(); //loads scene 
    void calculateFrameRate();

    /****** Engine Variables && Functions
     * 
     * 
     * 
     * */
    //Vars
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger; //can have as many of these debug messenger obj as we want
    DebugMessenger extDebug; //external debug trying to remove from code
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
    
    std::vector<VkFramebuffer> swapChainFramebuffers; //holds a framebuffer foreach VkImage in the swapchain
    
    VkCommandPool transientCommandPool; //stores our transient command pool (for short lived command buffers) //TESTING
    std::vector<VkFence> imagesInFlight; //used to check each swap chain image if a frame in flight is currently using it
    bool frameBufferResized = false; //explicitly track framebuffer resizeing, most platforms/drivers report a resize, but it's not gauranteed, so we can check
    size_t currentFrame = 0; //tracks the current frame based on MAX_FRAMES_IN_FLIGHT and acts as an index for imageAvailable and renderFinished semaphores
    uint32_t frameCounter = 0;

    uint32_t previousFrameCount = 0; //used to track framerate
    double previousFrameTime = 0; //used to track framerate

    std::vector<FrameData> _frames; //stores per frame variables like buffers and synchronization variables

    VkImage skyboxImage;
    VkImageView skyboxImageView;
    VmaAllocation skyboxAllocation;
    VkSampler skyboxSampler;
    VkDescriptorSet skyboxSet;
    VkPipeline skyboxPipeline;
    VkPipelineLayout _skyboxPipelineLayout;
    
    //Functions

    //Init
    void createVkInstance();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createMemAlloc();
    void createSwapChain();
    void createSwapChainImageViews();
    void createRenderPass();
    void createCommandBuffers();
    VkShaderModule createShaderModule(const std::vector<char>& code);
    void createFramebuffers();
    void createCommandPools();
    void init_pipelines(); //load and configure pipelines and correspending shader stages and assign to materials
    void createSyncObjects();
    void init_scene(); //assign and configure RenderObjects to display in the scene
    
    //Render Loop
    void drawObjects(int currentFrame);
    void rerecordCommandBuffer(int i);
    void mapLightingDataToGPU();
    void updateLightingData(GPUCameraData& camData);
    void updateSceneData(GPUCameraData& camData);
    
    //Cleanup
    void recreateSwapChain();
    void cleanupSwapChain();

    /****** Descriptor Variables, Buffers && functions
     * 
     * 
     * 
     * */
    //Vars
    //std::vector<VkDescriptorSet> _multiTextureDescriptorSets;
    VkDescriptorSetLayout _singleTextureSetLayout;
    VkDescriptorSetLayout _multiTextureSetLayout;
    VkDescriptorSetLayout _globalSetLayout; //handle to our global descriptor layout used for ubo, camera and sampler, set 0
    VkDescriptorSetLayout _objectSetLayout; //handle to our object descriptor layout, set 1
    VkDescriptorSetLayout _materialSetLayout; //handle to our object descriptor layout, set 1
    VkDescriptorSetLayout _lightingSetLayout; //handle to our object descriptor layout, set 1
    VkDescriptorSetLayout _skyboxSetLayout;
    VkDescriptorPool descriptorPool; //handle to our descriptor pool (need to read more about this)
    //Functions
    void createDescriptorSetLayouts();
    void createDescriptorPool();
    void createDescriptorSets();
    void createUniformBuffers();
    void allocateDescriptorSetForTexture(Material* material, std::string name);
    void allocateDescriptorSetForSkybox();
    //void allocateDescriptorSetForTexture(Material* material, std::string name);
    //void allocateDescriptorSetForMaterial(Material* material);

    /****** Reference Variables && functions
     * 
     * 
     * 
     * */
    //Vars
    WorldState* p_worldState;
    //Functions
    void populateCameraData(GPUCameraData& camData);
    void updateObjectTranslations();

    /****** Related Buffer Variables
     * 
     * 
     * 
     * */
    //Vars
    GPUSceneData _sceneParameters; //uniform buffer for scene variables for the shaders
	VkBuffer _sceneParameterBuffer;
    VmaAllocation _sceneParameterBufferAlloc;
    //GPUPointLightData _lightParameters; //uniform buffer for light variables for the shaders
	//VkBuffer _lightParameterBuffer;
    //VmaAllocation _lightParameterBufferAlloc;
    

    /****** Mesh Variables && Functions
     * 
     * These should be moved to meshes
     * 
     * */
    //vars
    std::unordered_map<std::string,Mesh> _meshes;
    std::vector<Mesh> _loadedMeshes;
    //these could get fed all the vertices and indices from each model
    VkBuffer vertexBuffer; //holds the vertex buffer handle
    VkBuffer indexBuffer; //holds the index buffer handle (for specifying vertices by index in the vertexBuffer)
    VmaAllocation vertexBufferAllocation; //stores the handle to the assigned memory on the GPU 
    VmaAllocation indexBufferAllocation; //stores the handle to the assigned memory on the GPU 
    std::vector<Vertex> allVertices;
    std::vector<uint32_t> allIndices;
    //functions
    void loadModels();
    //Mesh* get_mesh(const std::string& name);
    void populateVerticesIndices(std::string path, std::unordered_map<Vertex, uint32_t>& uniqueVertices, glm::vec3 baseColour);
    void createVertexBuffer(); //depends on vertex sizes and layout, might be able to overallocate?
    void createIndexBuffer(); //depends on index sizes and layout, might be able to overallocate?
    
    
    /****** Materials Variables && Functions
     * 
     * These should be moved to materials
     * 
     * */
    //Vars
    GPUMaterialData _materialParameters[MATERIALS_COUNT]; //GPU material data array thats uploaded via uniform buffer for shader referencing via push constant
	//VkBuffer _materialPropertiesBuffer; //these are currently stored per frame, may not be needed per frame but need to test
    //VmaAllocation _materialPropertiesBufferAlloc; //these are currently stored per frame, may not be needed per frame but need to test
    std::unordered_map<std::string,Material> _materials;
    //Functions
    Material* create_material(VkPipeline pipeline, VkPipelineLayout layout,const std::string& name, int id);
    Material* get_material(const std::string& name);
    void mapMaterialDataToGPU();

    /****** Texture Variables && Functions
     * 
     * These should be moved to textures
     * 
     * */
    //Vars
    std::unordered_map<std::string, Texture> _loadedTextures;
    std::unordered_map<std::string, Texture> _skyboxTextures;
    VkSampler diffuseSampler; //sampler
    VkSampler specularSampler; //sampler
    //VkImage textureImage; //holds the textureImage 
    //VmaAllocation textureImageAllocation; //handle to memory allocation
    //VkImageView textureImageView; //stores image view for the texture like the swap chain images and framebuffer, that images are accessed through views not directly
    //uint32_t mipLevels; //each mip image is stored in different mip levels of a VkImage, mip 0 is the original, others are part of mip chain
    //this is repeated per texture, we need to make structs and move code to textures.cpp/h
    //VkImage textureImage2; //holds the textureImage 
    //VmaAllocation textureImage2Allocation; //handle to memory allocation
    //VkImageView textureImage2View; //stores image view for the texture like the swap chain images and framebuffer, that images are accessed through views not directly
    //uint32_t mipLevels2;
    //Functions
    void createTextureImages();

    /****** Lighting Variables && Functions
     * 
     * These should be moved to materials
     * 
     * */
    //Vars
    GPUPointLightData _pointLightParameters[MAX_LIGHTS]; //GPU material data array thats uploaded via uniform buffer for shader referencing via push constant
    GPUSpotLightData _spotLightParameters[MAX_LIGHTS]; //GPU material data array thats uploaded via uniform buffer for shader referencing via push constant
    
    //void mapLightingDataToGPU();
    /****** Render Variables && Functions
     * 
     * 
     * 
     * */
    std::vector<RenderObject> _renderables;
    
    /****** Render image variables && Functions
     * 
     * 
     * 
     * */
    //Vars
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT; //MSAA, we will use a default of 1 sample per pixel which is no MSAA, but then check for support and adjsut at runtime
    VkImage depthImage; //depth image to draw to for calulcating depth
    VmaAllocation depthImageAllocation;
    VkImageView depthImageView;
    VkImage colourImage; //colour image to draw to for calulcating final output colour
    VmaAllocation colourImageAllocation;
    VkImageView colourImageView;
    std::vector<VkClearValue> clearValues; //stores clean values for clearing an image
    //Functions
    void createColourResources();
    void createDepthResources();
    //VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
    void createSamplers();
        
    /****** Helper Functions && Vars
     * 
     * 
     * 
     * */  
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size); //no need for public right now 
    UploadContext _uploadContext; //could be in public
    //void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);
    size_t pad_uniform_buffer_size(size_t originalSize);
    //struct for helping with vk object deletion, qeues up functions and executes them when flush() is called
    //so useful!
    
};

#endif /* !VK_ENGINE_D */