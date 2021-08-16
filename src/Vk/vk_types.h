#ifndef VK_TYPES_D
#define VK_TYPES_D
#include <vulkan/vulkan.h>
#include <array>
#define GLM_FORCE_RADIANS //makes sure GLM uses radians to avoid confusion
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES //forces GLM to use a version of vec2 and mat4 that have the correct alignment requirements for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE //forces GLM to use depth range of 0 to 1, instead of -1 to 1 as in OpenGL
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL //using glm to hash Vertex attributes using a template specialisation of std::hash<T>
#include <glm/gtx/hash.hpp> //used in unnordered_map to compare vertices
#include <vk_mem_alloc.h>
#include <string>

//struct for holding the point light data
struct GPUPointLightData {
	alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 ambient;
    alignas(16) glm::vec3 diffuse;
    alignas(16) glm::vec3 specular;
    alignas(16) glm::vec3 attenuation; // x - constant, y - linear, z - quadratic
};

//struct for holding the point light data
struct GPUSpotLightData {
	alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 ambient;
    alignas(16) glm::vec3 diffuse;
    alignas(16) glm::vec3 specular;
    alignas(16) glm::vec3 attenuation; // x - constant, y - linear, z - quadratic
    alignas(16) glm::vec3 direction; // z - is the spotlight angle
    alignas(16) glm::vec2 cutoffs; // z - is the spotlight angle
    //angles
};

//struct for holding the scene data
struct GPUSceneData {
	//glm::vec4 cameraPos; // w is ignored, no longer needed
	glm::vec4 fogDistances; //x for min, y for max, zw unused.
	glm::vec4 lightDirection; //w for sun power
    glm::vec4 lightAmbient;
    glm::vec4 lightDiffuse;
    glm::vec4 lightSpecular;
};

//struct for holding the cameradata on GPU for reading from vertex shader
struct GPUCameraData{
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 viewproj;
};

//will hold a properties for each material
struct GPUMaterialData {
    alignas(16) glm::vec3 diffuse;
    alignas(16) glm::vec3 specular;
    alignas(16) glm::vec3 extra;
};

//struct for holding all object matrices per frames
struct GPUObjectData{
	glm::mat4 modelMatrix;
    glm::mat4 normalMatrix;
};

//struct holding data for each frame (we are triple buffering so we have 3 of these)
struct FrameData {
    VkBuffer objectBuffer; //storage buffer for all object data in scene
    VmaAllocation objectBufferAlloc; 
    VkDescriptorSet objectDescriptor;

    VkBuffer materialBuffer; //storage buffer for all object materials in a scene
    VmaAllocation materialPropertiesBufferAlloc;
    VkDescriptorSet materialSet;

    VkDescriptorSet lightSet;
    VkBuffer pointLightParameterBuffer;
    VmaAllocation pointLightParameterBufferAlloc;
    VkBuffer spotLightParameterBuffer;
    VmaAllocation spotLightParameterBufferAlloc;

    VkBuffer cameraBuffer;
    VmaAllocation cameraBufferAllocation;

    VkDescriptorSet globalDescriptor;

    VkSemaphore _presentSemaphore, _renderSemaphore;
    VkFence _renderFence;	

    VkCommandPool _commandPool;
    VkCommandBuffer _mainCommandBuffer;
    VmaAllocation _mainCommandBufferAlloc;
};

//struct holding the push constants for the mesh
struct PushConstants {
	int matIndex; //we store an index that references material position in the _materialParameters array
    int numPointLights;
    int numSpotLights;
};

struct Texture {
	VkImage image;
    VmaAllocation alloc;
	VkImageView imageView;
    uint32_t mipLevels;
};

//Descriptor layout and buffer
//defining our ubo for use in the shader
//we can exactly match the definition in the shader using types in GLM
//the data in the matrices is binary compatible with the way the shader expects it, so we can jsut memcpy a UniformBufferObject to a VkBuffer
struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view; //view would be the same for every Objetct, it is the camera pos. It should be 
    alignas(16) glm::mat4 proj;
};

//struct for controlling uploads to cpu, short lived
struct UploadContext {
	VkFence _uploadFence;
	VkCommandPool _commandPool;	
};

//most Vulkan operations are submitted to queues, there are different types of queue for different queue families
    //to deal with this so lets use a struct to store them 
struct QueueFamilyIndices{
    //std optional is a std C++17 wrapper that contains no value until it is assigned
    //it can be queried with has_value(), this is because queue family indices can be any number including 0
    //so we cant check for failure with 0
    std::optional<uint32_t> graphicsFamily;

    //its possible that the queue families supporting drawing commands are the ones supporting presentation are not the same
    //so we need to check for presentation family queue support too
    std::optional<uint32_t> presentFamily;

    std::optional<uint32_t> transferFamily; //family for memory transfer

    //helper function that returns true if any value set
    bool isComplete(){
        //return graphicsFamily.has_value() && presentFamily.has_value();
        //additionally check we have a transferFamily
        return graphicsFamily.has_value() && presentFamily.has_value() && transferFamily.has_value(); 
    }
};

//just checking for swapchain support in device selection is not enough, because it may not be compatible with our window surface
//creating a swap chain also involes a lot more settings than instance and device creation, so we need to queury more details before we can proceed
//3 things to check :
//Basic surface capabilities (min/max number of images in chain, min/max width and height)
//Surface formats (pixel format and colour space)
//Available presentation modes
//Lets us a struct to pass these around in a similar fasion to findQueueFamilies
struct SwapChainSupportDetails{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

//this is a good idea but not used yet
//will hold allocation info, can be stored in each object so we can easily free it
//struct AllocatedBuffer {
//    VkBuffer _buffer;
//    VmaAllocation _allocation;
//};

#endif
