//#define VMA_DEBUG_LOG(format, ...) do { \
//       printf(format); \
//       printf("\n"); \
//  } while(false)
#define VMA_IMPLEMENTATION 
#include "vk_mem_alloc.h" //we are using VMA (Vulkan Memory Allocator to handle allocating blocks of memory for resources)
#include "world_state.h"
#include "vk_engine.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <optional>
#include <set>
#include <cstdint>
#include <algorithm>
#include <array>
#include <unordered_map>
#define GLM_FORCE_RADIANS //makes sure GLM uses radians to avoid confusion
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES //forces GLM to use a version of vec2 and mat4 that have the correct alignment requirements for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE //forces GLM to use depth range of 0 to 1, instead of -1 to 1 as in OpenGL
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL //using glm to hash Vertex attributes using a template specialisation of std::hash<T>
#include <glm/gtx/hash.hpp> //used in unnordered_map to compare vertices
#include <glm/gtc/matrix_transform.hpp> //view transformations like glm::lookAt, and projection transformations like glm::perspective glm::rotate
#define TINYOBJLOADER_IMPLEMENTATION //same as with STD, define in one source file to avoid linker errors
#include <tiny_obj_loader.h> //we will use tiny obj loader to load our meshes from file (could use fast_obj_loader.h if this is too slow)
#include "vk_debug_msg.h" //our seperated debug messenger, just to get some of the code out
#include "glfw_WindowHandler.h" //our window manager, just to get some of the code out
#include "vk_structs.h"
#include "vk_init_queries.h"
#include "vk_pipeline.h"
#include "vk_mesh.h"
#include "vk_images.h"

#include "world_state.h"
#include "ui_handler.h"

//static var declarations
VmaAllocator VulkanEngine::allocator;
QueueFamilyIndices VulkanEngine::queueFamilyIndicesStruct;// store the struct so we only call findQueueFamilies once
VkQueue VulkanEngine::graphicsQueue;
VkCommandPool VulkanEngine::transferCommandPool; //stores our transient command pool (for short lived command buffers) //TESTING
VkDevice VulkanEngine::device;
VkPhysicalDevice VulkanEngine::physicalDevice = VK_NULL_HANDLE; //stores the physical device handle

VulkanEngine::VulkanEngine(GLFWwindow* windowptr, WorldState* state): window{windowptr}, p_worldState{state}{}

void VulkanEngine::init(UiHandler* uiHandler){

    createVkInstance();
    if (enableValidationLayers){
        DebugMessenger::setupDebugMessenger(instance, &debugMessenger);
        _mainDeletionQueue.push_function([=]() {
		    DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	    });
    }
    //passes the instance and window and a pointer to surface to update the surface value
    windowHandler.createSurface(instance, window, &surface);

    pickPhysicalDevice();
    createLogicalDevice();
    createMemAlloc();

    createSwapChain();
    createSwapChainImageViews();
    createDescriptorSetLayouts();
    createCommandPools();
    createColourResources(); //this is our colourImage and resources for the render target for msaa
    createDepthResources();
    createUniformBuffers();
    createSamplers();

    createDescriptorPool();
    createDescriptorSets();

    createRenderPass();
    createFramebuffers();

    init_pipelines();
    createCommandBuffers();
    createSyncObjects();

    p_uiHandler = uiHandler;

    p_uiHandler->initUI(&device, &physicalDevice, &instance, queueFamilyIndicesStruct.graphicsFamily.value(), &graphicsQueue, &descriptorPool,
                (uint32_t)swapChainImages.size(), &swapChainImageFormat, &transferCommandPool, &swapChainExtent, &swapChainImageViews);
    

    //here we have now loaded the basics
    //this means we should be able to end the init() here and move the rest to a loadScene() method?
    loadScene();
}

//loads all scene data, this should be called only when the user hits run
void VulkanEngine::loadScene(){
    //next comes model loading and then creating vertex and index buffers on the gpu and copying the data
    loadModels();
    createVertexBuffer(); //error here if load models later
    createIndexBuffer(); //error here if load models later
    //now we can load the textures and create imageviews
    createTextureImages(); //pretty slow, probs texture number and size
    //creates RenderObjects and associates with WorldObjects, then allocates the textures 
    //this method should be split into two parts createRenderObjects and createTextureDescriptorSets 
    init_scene(); 
    mapMaterialDataToGPU();    
}

void VulkanEngine::allocateDescriptorSetForSkybox(){
    VkWriteDescriptorSet setWrite{};
    VkDescriptorImageInfo skyImageInfo;
    skyImageInfo.sampler = skyboxSampler;
    skyImageInfo.imageView = skyboxImageView; //this should exist before this call
    skyImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    setWrite = vkStructs::write_descriptorset(0, skyboxSet, 1, 
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &skyImageInfo);
    vkUpdateDescriptorSets(device,  1, &setWrite, 0, nullptr);
}
void VulkanEngine::allocateDescriptorSetForTexture(Material* material, std::string name){
    if(material != nullptr){
        
        material->_multiTextureSets.resize(1);

        //we also allocate our multiTextureSet here but not per frame and we dont write it yet
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.pNext = nullptr;
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 2;
        allocInfo.pSetLayouts = &_multiTextureSetLayout;

        vkAllocateDescriptorSets(device, &allocInfo, &material->_multiTextureSets[0]);

        std::array<VkWriteDescriptorSet, 2> setWrite{};

        VkDescriptorImageInfo diffImageInfo;
        diffImageInfo.sampler = diffuseSampler;
        diffImageInfo.imageView = _loadedTextures[name+"_diff"].imageView;
        diffImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo specImageInfo;
        specImageInfo.sampler = specularSampler;
        specImageInfo.imageView = _loadedTextures[name+"_spec"].imageView;
        specImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        setWrite[0] = vkStructs::write_descriptorset(0, material->_multiTextureSets[0], 1, 
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &diffImageInfo);

        setWrite[1] = vkStructs::write_descriptorset(1, material->_multiTextureSets[0], 1, 
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &specImageInfo);
        
        vkUpdateDescriptorSets(device,  setWrite.size(), setWrite.data(), 0, nullptr);
    }
}

void VulkanEngine::createSamplers(){
    //create a sampler for texture
	VkSamplerCreateInfo difSamplerInfo = vkStructs::sampler_create_info(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_TRUE, 16, 
        VK_BORDER_COLOR_INT_OPAQUE_BLACK, VK_FALSE, VK_COMPARE_OP_ALWAYS, VK_SAMPLER_MIPMAP_MODE_LINEAR, 0.0f, 0.0f, 
        static_cast<float>(_loadedTextures["satellite_diff"].mipLevels)); //this shouldnt be tied to the mip level of one texture, rather it should be all texture mip levels?
	vkCreateSampler(device, &difSamplerInfo, nullptr, &diffuseSampler);
    _mainDeletionQueue.push_function([=](){vkDestroySampler(device, diffuseSampler, nullptr);});

    VkSamplerCreateInfo specSamplerInfo = vkStructs::sampler_create_info(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_TRUE, 16, 
        VK_BORDER_COLOR_INT_OPAQUE_BLACK, VK_FALSE, VK_COMPARE_OP_ALWAYS, VK_SAMPLER_MIPMAP_MODE_LINEAR, 0.0f, 0.0f, 
        static_cast<float>(_loadedTextures["satellite_diff"].mipLevels)); //this shouldnt be tied to the mip level of one texture, rather it should be all texture mip levels?
	vkCreateSampler(device, &specSamplerInfo, nullptr, &specularSampler);
    _mainDeletionQueue.push_function([=](){vkDestroySampler(device, specularSampler, nullptr);});

    VkSamplerCreateInfo skySamplerInfo = vkStructs::sampler_create_info(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_TRUE, 16, 
        VK_BORDER_COLOR_INT_OPAQUE_BLACK, VK_FALSE, VK_COMPARE_OP_ALWAYS, VK_SAMPLER_MIPMAP_MODE_LINEAR, 0.0f, 0.0f, 1); //just 1 for now, 
	vkCreateSampler(device, &skySamplerInfo, nullptr, &skyboxSampler);
    _mainDeletionQueue.push_function([=](){vkDestroySampler(device, skyboxSampler, nullptr);});
}

void VulkanEngine::init_scene(){   
    _renderables.clear();    

    RenderObject skybox{p_worldState->objects[0]};
    skybox.meshId = get_mesh("box")->id;
    //box.material = get_material("defaultmesh");
    //box.material->diffuse = glm::vec3(0,0,1);
    //box.material->specular = glm::vec3(1,0.5f,0.5f);
    //box.material->extra.x = 32;
    _renderables.push_back(skybox);

    //to instanciate a RenderObject, it must be linked to a WorldState object
    RenderObject starSphere{p_worldState->objects[1]};
    starSphere.meshId = get_mesh("sphere")->id;
    starSphere.material = get_material("unlitmesh");
    starSphere.material->diffuse = glm::vec3(1,0.5f,0.31f);
    starSphere.material->specular = glm::vec3(0.5f,0.5f,0.5f);
    starSphere.material->extra.x = 32;
    _renderables.push_back(starSphere);

    //to instanciate a RenderObject, it must be linked to a WorldState object
    RenderObject sphereLight{p_worldState->pointLights[0]};
    sphereLight.meshId = get_mesh("sphere")->id;
    sphereLight.material = get_material("unlitmesh");
    sphereLight.material->diffuse = glm::vec3(1,0.5f,0.31f);
    sphereLight.material->specular = glm::vec3(0.5f,0.5f,0.5f);
    sphereLight.material->extra.x = 32;
    _renderables.push_back(sphereLight);

    //RenderObject sphere2{worldState.pointLights[1]};
    //sphere2.meshId = get_mesh("sphere")->id;
    //sphere2.material = get_material("unlitmesh");
    //_renderables.push_back(sphere2);

    RenderObject box{p_worldState->objects[2]};
    box.meshId = get_mesh("box")->id;
    box.material = get_material("defaultmesh");
    box.material->diffuse = glm::vec3(0,0,1);
    box.material->specular = glm::vec3(1,0.5f,0.5f);
    box.material->extra.x = 32;
    _renderables.push_back(box);

    //make a copy
    RenderObject box2{p_worldState->spotLights[0]};
    box2.meshId = get_mesh("box")->id;
    box2.material = get_material("unlitmesh");
    _renderables.push_back(box2);

    RenderObject satellite{p_worldState->objects[3]};
    satellite.meshId = get_mesh("satellite")->id;
    satellite.material = get_material("texturedmesh2");
    satellite.material->extra.x = 2048;
    _renderables.push_back(satellite);

    RenderObject asteroid{p_worldState->objects[4]};
    asteroid.meshId = get_mesh("asteroid")->id;
    asteroid.material = get_material("texturedmesh1"); //comment out
    //asteroid.material = get_material("defaultmesh");
    //asteroid.material->diffuse = glm::vec3(0.5f,0.5f,0.5f);
    //asteroid.material->specular = glm::vec3(1,1,1);
    asteroid.material->extra.x = 2048;

    //asteroid.material->extra.x = 32;
    _renderables.push_back(asteroid);

    for(auto iter : _materials){
        _materialParameters[iter.second.propertiesId].diffuse = iter.second.diffuse;
        _materialParameters[iter.second.propertiesId].specular = iter.second.specular;
        _materialParameters[iter.second.propertiesId].extra = iter.second.extra;
    }

    //allocate a descriptor set for each material pointing to each texture needed
    allocateDescriptorSetForTexture(get_material("texturedmesh2"), "satellite");
    allocateDescriptorSetForTexture(get_material("texturedmesh1"), "asteroid");
    allocateDescriptorSetForSkybox(); //skybox hs its own descriptor set because it behaves differently from normal texture
}

void VulkanEngine::updateObjectTranslations(){
    //temp code, scaling fixed and _renderables/WorldObjects may need changed
    glm::mat4 scale;
    glm::mat4 translation;
    glm::mat4 rotation;
    for (int i = 0; i < _renderables.size(); i++){
        scale = glm::scale(glm::mat4{ 1.0 }, _renderables[i].rWorldObject.scale);
        translation = glm::translate(glm::mat4{ 1.0 }, _renderables[i].rWorldObject.pos);
        rotation = _renderables[i].rWorldObject.rot;
         _renderables[i].transformMatrix = translation * rotation * scale;
    }
}

void VulkanEngine::populateCameraData(GPUCameraData& camData){
    WorldCamera* worldCam = p_worldState->getWorldCamera();
    glm::vec3 camPos = worldCam->cameraPos;
    glm::mat4 view = glm::lookAt(camPos, camPos + worldCam->cameraFront, worldCam->cameraUp);
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float) swapChainExtent.height, 0.1f, 15000.0f);
    //GLM was originally designed for OpenGL, where the Y coordinate of the clip coordinates is inverted, so we simply flip it
    //view[1][1] *= -1; //<--- really trippy :)
    proj[1][1] *= -1;
    camData.projection = proj;
    camData.view = view;
	camData.viewproj = proj * view;
}

void VulkanEngine::mapMaterialDataToGPU(){ //not sure i need 3 of these if they were going to be read only anyway, test at some point
    for(int i = 0; i < swapChainImages.size(); i++){
        //copy material data array into position, can i do this outside of the render loop as it will be static? lets try
        char* materialData;
        vmaMapMemory(allocator, _frames[i].materialPropertiesBufferAlloc , (void**)&materialData);
        memcpy(materialData, &_materialParameters, sizeof(_materialParameters));
        vmaUnmapMemory(allocator, _frames[i].materialPropertiesBufferAlloc); 
    }
}

void VulkanEngine::mapLightingDataToGPU(){
    for(int i = 0; i < swapChainImages.size(); i++){
        //copy current point light data array into buffer
        char* pointLightingData;
        vmaMapMemory(allocator, _frames[i].pointLightParameterBufferAlloc , (void**)&pointLightingData);
        memcpy(pointLightingData, &_pointLightParameters, sizeof(_pointLightParameters));
        vmaUnmapMemory(allocator, _frames[i].pointLightParameterBufferAlloc); 
    }
    for(int i = 0; i < swapChainImages.size(); i++){
        //copy current spot light data array into buffer
        char* spotLightingData;
        vmaMapMemory(allocator, _frames[i].spotLightParameterBufferAlloc , (void**)&spotLightingData);
        memcpy(spotLightingData, &_spotLightParameters, sizeof(_spotLightParameters));
        vmaUnmapMemory(allocator, _frames[i].spotLightParameterBufferAlloc); 
    }
}

void VulkanEngine::updateSceneData(GPUCameraData& camData){
    WorldLightObject& sceneLight = p_worldState->scenelight;
    _sceneParameters.lightDirection = glm::vec4(camData.view * glm::vec4(sceneLight.pos, 0));
    _sceneParameters.lightAmbient = glm::vec4(sceneLight.ambient, 1);
    _sceneParameters.lightDiffuse = glm::vec4(sceneLight.diffuse, 1);
    _sceneParameters.lightSpecular = glm::vec4(sceneLight.specular, 1);
}

//point lights
void VulkanEngine::updateLightingData(GPUCameraData& camData){
    std::vector<WorldPointLightObject>& worldPointLights = p_worldState->getWorldPointLightObjects();
    for(int i = 0; i < worldPointLights.size(); i++){
        _pointLightParameters[i].position = glm::vec3(camData.view * glm::vec4(worldPointLights[i].pos, 1));
        _pointLightParameters[i].diffuse = worldPointLights[i].diffuse;
        _pointLightParameters[i].ambient = worldPointLights[i].ambient;
        _pointLightParameters[i].specular = worldPointLights[i].specular;
        _pointLightParameters[i].attenuation = worldPointLights[i].attenuation;
    }
    std::vector<WorldSpotLightObject>& worldSpotLights = p_worldState->getWorldSpotLightObjects();
    for(int i = 0; i < worldSpotLights.size(); i++){
        _spotLightParameters[i].position = glm::vec3(camData.view * glm::vec4(worldSpotLights[i].pos, 1));
        _spotLightParameters[i].diffuse = worldSpotLights[i].diffuse;
        _spotLightParameters[i].ambient = worldSpotLights[i].ambient;
        _spotLightParameters[i].specular = worldSpotLights[i].specular;
        _spotLightParameters[i].attenuation = worldSpotLights[i].attenuation;
        _spotLightParameters[i].direction = glm::vec3(camData.view * glm::vec4(worldSpotLights[i].direction, 0));
        _spotLightParameters[i].cutoffs = worldSpotLights[i].cutoffs;
    }
}

void VulkanEngine::drawObjects(int curFrame){
    ////fill a GPU camera data struct
	GPUCameraData camData;
    populateCameraData(camData);
    updateSceneData(camData);
    
    //and copy it to the buffer
	void* data;
	vmaMapMemory(allocator, _frames[curFrame].cameraBufferAllocation, &data);
	memcpy(data, &camData, sizeof(GPUCameraData));
	vmaUnmapMemory(allocator, _frames[curFrame].cameraBufferAllocation);

    //UPDATE TRANSFORM MATRICES OF ALL MODELS HERE
    updateObjectTranslations();

    //fetch latest lighting data
    updateLightingData(camData);
    //map it to the GPU
    mapLightingDataToGPU();
    
    //then do the objects data into the storage buffer
    void* objectData;
    vmaMapMemory(allocator, _frames[curFrame].objectBufferAlloc, &objectData);
    GPUObjectData* objectSSBO = (GPUObjectData*)objectData;
    //loop through all objects in the scene, need a better container than a vector
    for (int i = 0; i < _renderables.size(); i++){
        objectSSBO[i].modelMatrix = _renderables[i].transformMatrix;
        //calculate the normal matrix, its done by inverse transpose of the model matrix * view matrix because we are working 
        //in view space in the shader. taking only the top 3x3
        //this is to account for rotation and scaling when using normals. should be done on cpu as its costly
        objectSSBO[i].normalMatrix = glm::transpose(glm::inverse(camData.view * _renderables[i].transformMatrix)); 
    }
    vmaUnmapMemory(allocator, _frames[curFrame].objectBufferAlloc);
  
	char* sceneData;
    vmaMapMemory(allocator, _sceneParameterBufferAlloc , (void**)&sceneData);
	int frameIndex = frameCounter % MAX_FRAMES_IN_FLIGHT;
	sceneData += pad_uniform_buffer_size(sizeof(GPUSceneData)) * frameIndex; 
	memcpy(sceneData, &_sceneParameters, sizeof(GPUSceneData));
	vmaUnmapMemory(allocator, _sceneParameterBufferAlloc); 

    Material* lastMaterial = nullptr;
    for (int i = 1; i < _renderables.size(); i++){
        RenderObject& object = _renderables.data()[i];
        //only bind the pipeline if it doesn't match with the already bound one
		if (object.material != lastMaterial) {
            if(object.material == nullptr){throw std::runtime_error("object.material is a null reference in drawObjects()");} //remove if statement in release
			vkCmdBindPipeline(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline);
			lastMaterial = object.material;

            //offset for our scene buffer
            uint32_t scene_uniform_offset = pad_uniform_buffer_size(sizeof(GPUSceneData)) * frameIndex;
            //bind the descriptor set when changing pipeline
            vkCmdBindDescriptorSets(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                object.material->pipelineLayout, 0, 1, &_frames[curFrame].globalDescriptor, 1, &scene_uniform_offset);

	        //object data descriptor
        	vkCmdBindDescriptorSets(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                object.material->pipelineLayout, 1, 1, &_frames[curFrame].objectDescriptor, 0, nullptr);

             //material descriptor, we are using a dynamic uniform buffer and referencing materials by their offset with propertiesId, which just stores the int relative to the material in _materials
        	vkCmdBindDescriptorSets(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                object.material->pipelineLayout, 2, 1, &_frames[curFrame].materialSet, 0, nullptr);
            //the "firstSet" param above is 2 because in init_pipelines its described in VkDescriptorSetLayout[] in index 2!
            
            vkCmdBindDescriptorSets(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                object.material->pipelineLayout, 3, 1, &_frames[curFrame].lightSet, 0, nullptr);

            if (object.material->_multiTextureSets.size() > 0) {
                vkCmdBindDescriptorSets(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                    object.material->pipelineLayout, 4, 1, &object.material->_multiTextureSets[0], 0, nullptr);
		    }
		}

        //add material property id for this object to push constant
		PushConstants constants;
		constants.matIndex = object.material->propertiesId;
        constants.numPointLights = p_worldState->pointLights.size();
        constants.numSpotLights = p_worldState->spotLights.size();

        //upload the mesh to the GPU via pushconstants
		vkCmdPushConstants(_frames[curFrame]._mainCommandBuffer, object.material->pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &constants);

        vkCmdDrawIndexed(_frames[curFrame]._mainCommandBuffer, _loadedMeshes[object.meshId].indexCount, 1, _loadedMeshes[object.meshId].indexBase, 0, i); //using i as index for storage buffer in shaders

        //now we draw the skybox last
        //vkCmdDraw()
    }

    uint32_t scene_uniform_offset = pad_uniform_buffer_size(sizeof(GPUSceneData)) * frameIndex;
    vkCmdBindDescriptorSets(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _skyboxPipelineLayout, 0, 1, &_frames[curFrame].globalDescriptor, 1, &scene_uniform_offset);
    vkCmdBindDescriptorSets(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _skyboxPipelineLayout, 1, 1, &_frames[curFrame].objectDescriptor, 0, nullptr);
    vkCmdBindDescriptorSets(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _skyboxPipelineLayout, 2, 1, &skyboxSet, 0, NULL);
	vkCmdBindPipeline(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline);
	vkCmdDrawIndexed(_frames[curFrame]._mainCommandBuffer, _loadedMeshes[_renderables[0].meshId].indexCount, 1, _loadedMeshes[_renderables[0].meshId].indexBase, 0, 0);
}


void VulkanEngine::rerecordCommandBuffer(int i){
    vkResetCommandBuffer(_frames[i]._mainCommandBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT); 
    
    VkCommandBufferBeginInfo beginInfo = vkStructs::command_buffer_begin_info(0);
    if(vkBeginCommandBuffer(_frames[i]._mainCommandBuffer, &beginInfo) != VK_SUCCESS){
        throw std::runtime_error("Failed to begin recording command buffer");
    }
   
    //note that the order of clear values should be identical to the order of attachments
    VkRenderPassBeginInfo renderPassBeginInfo = vkStructs::render_pass_begin_info(renderPass, swapChainFramebuffers[i], {0, 0}, swapChainExtent, clearValues);

    vkCmdBeginRenderPass(_frames[i]._mainCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
   
    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    
    if(!_renderables.empty()){
        vkCmdBindVertexBuffers(_frames[i]._mainCommandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(_frames[i]._mainCommandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        drawObjects(i);
    }
    
    //we we can end the render pass
    vkCmdEndRenderPass(_frames[i]._mainCommandBuffer);

     //and we have finished recording, we check for errrors here
    if (vkEndCommandBuffer(_frames[i]._mainCommandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }

    //now for the UI render pass
    vkResetCommandBuffer(p_uiHandler->guiCommandBuffers[i], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT); 
    
    beginInfo = vkStructs::command_buffer_begin_info(0);
    if(vkBeginCommandBuffer(p_uiHandler->guiCommandBuffers[i], &beginInfo) != VK_SUCCESS){
        throw std::runtime_error("Failed to begin recording gui command buffer");
    }

    p_uiHandler->drawUI(); //then we will draw UI

    //note that the order of clear values should be identical to the order of attachments 
    renderPassBeginInfo = vkStructs::render_pass_begin_info(p_uiHandler->guiRenderPass, p_uiHandler->guiFramebuffers[i], {0, 0}, swapChainExtent, clearValues);
    vkCmdBeginRenderPass(p_uiHandler->guiCommandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Record Imgui Draw Data and draw funcs into command buffer
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), p_uiHandler->guiCommandBuffers[i]);

    //we we can end the render pass
    vkCmdEndRenderPass(p_uiHandler->guiCommandBuffers[i]);

     //and we have finished recording, we check for errrors here
    if (vkEndCommandBuffer(p_uiHandler->guiCommandBuffers[i]) != VK_SUCCESS) {
        throw std::runtime_error("failed to record gui command buffer!");
    }
}
 
void VulkanEngine::drawFrame(){
    
    vkWaitForFences(device, 1, &_frames[currentFrame]._renderFence, VK_TRUE, UINT64_MAX); 
    
    //so the first step is to acquire an image from the swap chain
    uint32_t imageIndex;
    //we will store the return rsult of the call VkResult and use it to check if the swap chain needs recreated if its out of date
    VkResult result = vkAcquireNextImageKHR(device,swapChain, UINT64_MAX, _frames[currentFrame]._presentSemaphore, VK_NULL_HANDLE, &imageIndex);
    if(result == VK_ERROR_OUT_OF_DATE_KHR){
        recreateSwapChain();
        return;
    }
    else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR){
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    //before we continue using the frame we should check to make sure it is not in use in the chain somewhere
    if(imagesInFlight[imageIndex] != VK_NULL_HANDLE){
        vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    //once we are clear we assign this inFlightFence to imagesInFlight so we can keep it locked until this pass is done for this image
    imagesInFlight[imageIndex] = _frames[currentFrame]._renderFence;

    //we have the imageIndex and are cleared for operations on it so it should be safe to rerecord the command buffer here
    rerecordCommandBuffer(imageIndex);

    VkSemaphore waitSemaphores[] = {_frames[currentFrame]._presentSemaphore}; 
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}; 
    VkSemaphore signalSemaphores[] = {_frames[currentFrame]._renderSemaphore};

    //Submitting the command buffer
    //queue submission and synchronization is configured through VkSubmitInfo struct
    VkCommandBuffer buffers[] = {_frames[imageIndex]._mainCommandBuffer, p_uiHandler->guiCommandBuffers[imageIndex]};
    VkSubmitInfo submitInfo = vkStructs::submit_info(buffers, 2, waitSemaphores, 1, 
        waitStages, signalSemaphores, 1);
    
    //move our reset fences here, its best to simply reset a fence right before its used
    vkResetFences(device, 1, &_frames[currentFrame]._renderFence); //unlike semaphores, fences must be manually reset

    //submit the queue
    if(vkQueueSubmit(graphicsQueue, 1 , &submitInfo, _frames[currentFrame]._renderFence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer");
    }

    //Presentation
    //The last step of drawing a frame is submitting the result back to the swapchain
    VkSwapchainKHR swapChains[] = {swapChain};
    VkPresentInfoKHR presentInfo = vkStructs::present_info(&_frames[currentFrame]._renderSemaphore, 1, swapChains, &imageIndex);

    //now we submit the request to present an image to the swapchain. We'll add error handling later, as failure here doesnt necessarily mean we should stop
    result = vkQueuePresentKHR(presentQueue, &presentInfo);
    //in addition to these checks we also check member variable frameBufferResized for manual confirmation
    //must do this after vkQueuePresentKHR() call to ensure semaphores are in a consinstent state, otherwise a signalled semaphore may never be properly waited on
    //to actually manually detect resizes we use glfwSetFramebufferSizeCallback() to set a callback in initWindow()
    //if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || frameBufferResized){
    if(result == VK_ERROR_OUT_OF_DATE_KHR){ //ignoring frameBufferResized and VK_SUBOPTIMAL_KHR stops race condition causing validation errors, not ideal
        //frameBufferResized = false;
        recreateSwapChain();
    }
    else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR){
        throw std::runtime_error("Failed to acquire swapchain image");
    }
    frameCounter++;
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

    calculateFrameRate();
}

void VulkanEngine::calculateFrameRate(){
    double currentFrameTime = glfwGetTime();
    if(currentFrameTime - previousFrameTime >= 1.0 ){ // If last update was more than 1 sec ago
        renderStats.framerate = 1000.0/double(frameCounter-previousFrameCount);
        renderStats.fps = frameCounter-previousFrameCount;
        previousFrameTime += 1.0;
        previousFrameCount = frameCounter;
     }
}

void VulkanEngine::cleanup(){
    vkDeviceWaitIdle(device); //maske sure device is idle and not mid draw

    cleanupSwapChain();

    p_uiHandler->cleanup();

    _mainDeletionQueue.flush();

    vkDestroySurfaceKHR(instance, surface, nullptr); //surface should be destroyed before instance
    vkDestroyInstance(instance, nullptr); //all other vulkan resources should be cleaned u//helper function that gets max usable sample count for MSAA
    glfwDestroyWindow(window); //then destroy window
    glfwTerminate(); //then glfw
}

void VulkanEngine::createVkInstance()
{
    if (enableValidationLayers && !init_query::checkValidationLayerSupport(validationLayers))
    {
        throw std::runtime_error("Validation layers requested, but not available.");
    }

    VkApplicationInfo appInfo = vkStructs::app_info("Vulkan Testing App Name");
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo; //also set up debug callbacks for create and destroy
    uint32_t enabledLayerCount = 0;
    const char *const *enabledLayerNames; //wtf 
    void * next = nullptr;
    if (enableValidationLayers)
    {
        enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        enabledLayerNames = validationLayers.data();
        extDebug.populateDebugMessengerCreateInfo(debugCreateInfo);
        next = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
    }
    auto extensions = init_query::getRequiredExtensions(enableValidationLayers);
    VkInstanceCreateInfo createInfo = vkStructs::instance_create_info(appInfo, extensions, enabledLayerNames, enabledLayerCount, next);

    //call create instance, passing pointer to createInfo and instance, instance handle is stored in VKInstance instance
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create instance!");
    }
}

//picks physical device, moved most of the queries to init_query namespace in vk_init_queries.h
void VulkanEngine::pickPhysicalDevice(){
    //query the number of devices with vulkan support
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if(deviceCount == 0) {
        throw std::runtime_error("Failed to find GPU's with Vulkan support");
    }

    //query the list of devices
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    //check each device is suitable, stop at first suitable device
    for(const auto& device : devices){
        if(init_query::isDeviceSuitable(device, surface, deviceExtensions)){
            physicalDevice = device;
            msaaSamples = init_query::getMaxUsableSampleCount(physicalDevice); //check our max sample count supported
            _gpuProperties = init_query::getPhysicalDeviceProperties(physicalDevice);
            break;
        }
    }

    if(physicalDevice == VK_NULL_HANDLE){
        throw std::runtime_error("Failed to find suitable GPU");
    }
}

//after selecting a physical device we need to create a logical device to interface with it
//also need to specify which queues we want to create now that we've queuried available queue families
void VulkanEngine::createLogicalDevice(){
    //we need to have multiple VkDeviceQueueCreateInfo structs to create a queue for both families

    //find and store the queue family indices for the physical device we picked
    queueFamilyIndicesStruct = init_query::findQueueFamilies(physicalDevice, surface);

    std::set<uint32_t> uniqueQueueFamilies = {queueFamilyIndicesStruct.graphicsFamily.value(), 
        queueFamilyIndicesStruct.presentFamily.value(), queueFamilyIndicesStruct.transferFamily.value()};

    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos = vkStructs::queue_create_infos(uniqueQueueFamilies, &queuePriority);

    //currently available drivers only support a small number of queues for each family,
    //and we dont really need mroe than one because you can create all of the command buffers omn multiple threads
    //and then submit them all at once on the main thread with a single low-overhead call
    //initialise this structure all as VK_FALSE for now, come back to this later
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE; //we want to enable the sampler anisotropy features for our sampler

    //with the previous 2 structs in place we can begin filling in the main VkDeviceCreateInfo struct
    VkDeviceCreateInfo createInfo = vkStructs::device_create_info(deviceFeatures, queueCreateInfos, deviceExtensions);
    
    //now we are ready to instantiate the logical device with a call to vkCreateDevice
    // params are the physical device to interface with, the queue and usage info we just specified,
    //the optional callback and a pointer to a variable to store the logical device handle
    if(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }

    //we can use this function to retrieve the queue handles for each queue family
    //pamas are the logical device, queue family, queue index and a pointer to store the handle in
    //because we are only creating a single queue we will use 0 for index
    vkGetDeviceQueue(device, queueFamilyIndicesStruct.graphicsFamily.value(), 0 , &graphicsQueue);

    //request queue handle for presentation queue
    vkGetDeviceQueue(device, queueFamilyIndicesStruct.presentFamily.value(), 0 , &presentQueue);

    //finally we want to retrieve our transferQueue handle
    vkGetDeviceQueue(device, queueFamilyIndicesStruct.transferFamily.value(), 0 , &transferQueue);

    _mainDeletionQueue.push_function([=]() {
		vkDestroyDevice(device, nullptr);
	});
}

void VulkanEngine::createMemAlloc(){
    VmaAllocatorCreateInfo allocatorInfo = vkStructs::vma_allocator_info(physicalDevice, device, instance, allocator);  
    vmaCreateAllocator(&allocatorInfo, &allocator);
    _mainDeletionQueue.push_function([=](){vmaDestroyAllocator(allocator);});
}

//now we can use all our helper functions to create a swap chain based on their choices
void VulkanEngine::createSwapChain(){
    SwapChainSupportDetails swapChainSupport = init_query::querySwapChainSupport(physicalDevice, surface);
    VkSurfaceFormatKHR surfaceFormat = init_query::chooseSwapSurfaceFormat(swapChainSupport.formats); //HERE
    VkPresentModeKHR presentMode = init_query::chooseSwapPresentMode(swapChainSupport.presentModes);

    VkExtent2D extent = init_query::chooseSwapExtent(swapChainSupport.capabilities, window);
    //we also need to set the number of images we would like in the swap chain
    //it is recommended to do at least one more than the minimum 
    //so that if the driver is working on internal operations we can acquire another to render to
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    //we must also check that we dont exceed the maximum by doing this, maxImageCount special value of 0 means there is no maximum
    //so if there is a maximum and we exceed the max, set imageCount to max
    if(swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount){
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }
    uint32_t queueFamilyIndices[] = {queueFamilyIndicesStruct.graphicsFamily.value(), queueFamilyIndicesStruct.presentFamily.value()};
   
    VkSwapchainCreateInfoKHR createInfo = vkStructs::swapchain_create_info(surface, imageCount, surfaceFormat, extent, queueFamilyIndices, swapChainSupport.capabilities.currentTransform, presentMode);

    //now we can create the swap chain with the call
    //params are logical device, the swapchain creation info, optional custom allocators, and a pointer to the variable to store the handle
    if(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS){
        throw std::runtime_error("Failed to create swap chain");
    }
    _swapDeletionQueue.push_function([=](){vkDestroySwapchainKHR(device, swapChain, nullptr);});

    //now we need get the handles of the VkImages created in the blockchain
    //first we should get the amount that were created and resize the array accordingly (we only specified minimum to create earlier)
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

    //finally store the format and extent we've chosen for the swap chain for later use.
    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;

    _frames.resize(swapChainImages.size());
}

//VkImageViews describe how to access VkImage such as those in the swap chain and which part of the image to use
void VulkanEngine::createSwapChainImageViews(){
    //first resize the array to the required size
    swapChainImageViews.resize(swapChainImages.size());
    //loop through all swap chain iamges
    for(size_t i = 0; i < swapChainImages.size(); i++){
        swapChainImageViews[i] = image_func::createImageView(swapChainImages[i], VK_IMAGE_VIEW_TYPE_2D, swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1);
        _swapDeletionQueue.push_function([=](){vkDestroyImageView(device, swapChainImageViews[i], nullptr);});
    }
}    

//before we can finish creating the pipeline, we need to tell vulkan about the framebuffer attachments that will be used while rendering
//How many colour and depth buffers there will be, how many samples of each and how their contents should be handled throughout rendering ops
void VulkanEngine::createRenderPass(){
    //in our case we will need a colour buffer represented by one of the images from the swap chain
    VkAttachmentDescription colourAttachment = vkStructs::attachment_description(swapChainImageFormat, msaaSamples, VK_ATTACHMENT_LOAD_OP_CLEAR, 
        VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    //Subpasses and attachment references
    //A single rendering pass can consist of multiple subpasses. Subpasses are sunsequent rendering operations that depend on contents of
    //framebuffers in previous passes. Eg a sequence of post processing effects that are applied one after another
    //if you group these operations onto one render pass then Vulkan is able to optimize.
    //For our first triangle we will just have one subpass.
    
    //Every subpass references one or more of the attachments described in the previous sections. These references are VkAttachmentReference structs
    VkAttachmentReference colourAttachmentRef = vkStructs::attachment_reference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    //because we need to resolve the above multisampled image before presentation (you cant present a multisampled image because
    //it contains multiple pixel values per pixel) we need an attachment that will resolve the image back to something we can 
    //VK_IMAGE_LAYOUT_PRESENT_SRC_KHR and in standard 1 bit sampling
    //VkAttachmentDescription colourAttachmentResolve = vkStructs::attachment_description(swapChainImageFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_DONT_CARE, 
    //    VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VkAttachmentDescription colourAttachmentResolve = vkStructs::attachment_description(swapChainImageFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_DONT_CARE, 
        VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL); //CHANGED TO VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL as it is not the final layout any more, becaus ethe GUI will be drawn in another subpass later
    
    //the render pass now has to be instructed to resolve the multisampled colour image into the regular attachment
    VkAttachmentReference colourAttachmentResolveRef = vkStructs::attachment_reference(2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    // why is that index 2 and not 1? need to check

    // we also want to add a Subpass Dependency as described in drawFrame() to wait for VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT before proceeding 
    //specify indices of dependency and dependent subpasses, 
    //special value VK_SUBPASS_EXTERNAL refers to implicit subpass before or after the the render pass depending on whether specified in srcSubpass/dstSubpass
    //The operations that should waiti on this are in the colour attachment stage and involve the writing of the colour attachment
    //these settings will prevent the transition from happening until its actually necessary and allowed, ie when we want to start writing colours to it
    VkSubpassDependency dependency = vkStructs::subpass_dependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    
    //we want to include a depth attachment for depth testing
    // VK_ATTACHMENT_STORE_OP_DONT_CARE; //we dont need to store depth data as it will be discarded every pass
    // VK_IMAGE_LAYOUT_UNDEFINED; //same as colour buffer, we dont care about previous depth contents
    VkAttachmentDescription depthAttachment = vkStructs::attachment_description(init_query::findDepthFormat(physicalDevice), msaaSamples, VK_ATTACHMENT_LOAD_OP_CLEAR, 
        VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
 
    VkAttachmentReference depthAttachmentRef = vkStructs::attachment_reference(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    // and here is index 1, just described in wrong order

    //we add a reference to the attachment for the first (and only) subpass
    //the index of the attachment in this array is directly referenced from the fragment shader with the layout(location = 0)out vec4 outColor directive.
    //we pass in the colourAttachmentResolveRef struct which will let the render pass define a multisample resolve operation
    //described in the struct, which will let us render the image to screen by resolving it to the right layout
    //pDepthStencilAttachment: Attachment for depth and stencil data        
    VkSubpassDescription subpass = vkStructs::subpass_description(VK_PIPELINE_BIND_POINT_GRAPHICS, 1, &colourAttachmentResolveRef, &colourAttachmentRef, &depthAttachmentRef);

    //now we udpate the VkRenderPassCreateInfo struct to refer to both attachments 
    //and the colourAttachmentResolve
    std::vector<VkAttachmentDescription> attachments = {colourAttachment, depthAttachment, colourAttachmentResolve};
    VkRenderPassCreateInfo renderPassInfo = vkStructs::renderpass_create_info(attachments, 1, &subpass, 1, &dependency);

    if(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS){
        throw std::runtime_error("Failed to create render pass");
    }
    _swapDeletionQueue.push_function([=](){vkDestroyRenderPass(device, renderPass, nullptr);});
}

//ubo Descriptor layout
//we need to provide details about every descriptor binding used in the shader for pipeline creation. Just like we had to do for every vertrex attribute
//and its location index. Because we will need to use it in pipeline creation we will do this first
void VulkanEngine::createDescriptorSetLayouts(){

    //Create Descriptor Set layout describing the _globalSetLayout containing global scene and camera data
    //Cam is set 0 binding 0 because its first in this binding set
    //Scene is set 0 binding 1 because its second in this binding set
    VkDescriptorSetLayoutBinding camBufferBinding = vkStructs::descriptorset_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr);
    //binding for scene data at 1 in vertex and frag
	VkDescriptorSetLayoutBinding sceneBind = vkStructs::descriptorset_layout_binding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    std::vector<VkDescriptorSetLayoutBinding> bindings =  {camBufferBinding, sceneBind};
    VkDescriptorSetLayoutCreateInfo setinfo = vkStructs::descriptorset_layout_create_info(bindings);
    if(vkCreateDescriptorSetLayout(device, &setinfo, nullptr, &_globalSetLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create descriptor set layout");
    }
    _mainDeletionQueue.push_function([=](){vkDestroyDescriptorSetLayout(device, _globalSetLayout, nullptr);});

    //create a descriptorset layout for material
    //we will create the descriptor set layout for the objects as a seperate descriptor set
    //bind object storage buffer to 0 in vertex
    //this needs to be binding 1 because in allocatedescset for materials we add it to the write set in the second position (after texture set)
    //VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    VkDescriptorSetLayoutBinding materialsBind = vkStructs::descriptorset_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    bindings.clear();
    bindings = {materialsBind};
	setinfo = vkStructs::descriptorset_layout_create_info(bindings);
    if(vkCreateDescriptorSetLayout(device, &setinfo, nullptr, &_materialSetLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create descriptor set layout");
    }
    _mainDeletionQueue.push_function([=](){vkDestroyDescriptorSetLayout(device, _materialSetLayout, nullptr);});

    //we will create the descriptor set layout for the objects as a seperate descriptor set
    //bind object storage buffer to 0 in vertex
    VkDescriptorSetLayoutBinding objectBind = vkStructs::descriptorset_layout_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr);
    bindings.clear();
    bindings = {objectBind};
	setinfo = vkStructs::descriptorset_layout_create_info(bindings);
    if(vkCreateDescriptorSetLayout(device, &setinfo, nullptr, &_objectSetLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create descriptor set layout");
    }
    _mainDeletionQueue.push_function([=](){vkDestroyDescriptorSetLayout(device, _objectSetLayout, nullptr);});

    //we will create the descriptor set layout for the lights, could maybe be combined with materials? or object?
    //bind light storage buffer to 0 in frag
    VkDescriptorSetLayoutBinding pointLightsBind = vkStructs::descriptorset_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    VkDescriptorSetLayoutBinding spotLightsBind = vkStructs::descriptorset_layout_binding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    bindings.clear();
    bindings = {pointLightsBind, spotLightsBind};
	setinfo = vkStructs::descriptorset_layout_create_info(bindings);
    if(vkCreateDescriptorSetLayout(device, &setinfo, nullptr, &_lightingSetLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create descriptor set layout");
    }
    _mainDeletionQueue.push_function([=](){vkDestroyDescriptorSetLayout(device, _lightingSetLayout, nullptr);});

    //then we will create the descriptor set layout for the sampler as another seperate descriptor set
    //bind samplerBuffer to 0 in frag 
    VkDescriptorSetLayoutBinding textureLayoutBinding = vkStructs::descriptorset_layout_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    bindings.clear(); //clean this up no need for vectors here
    bindings =  {textureLayoutBinding};
	setinfo = vkStructs::descriptorset_layout_create_info(bindings);
    if(vkCreateDescriptorSetLayout(device, &setinfo, nullptr, &_singleTextureSetLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create sampler texture set layout");
    }
    _mainDeletionQueue.push_function([=](){vkDestroyDescriptorSetLayout(device, _singleTextureSetLayout, nullptr);});

    VkDescriptorSetLayoutBinding diffuseSetBinding = vkStructs::descriptorset_layout_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    //binding for scene data at 1 in vertex and frag
	VkDescriptorSetLayoutBinding specularSetBinding = vkStructs::descriptorset_layout_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    bindings.clear();
    bindings =  {diffuseSetBinding, specularSetBinding};
    setinfo = vkStructs::descriptorset_layout_create_info(bindings);
    if(vkCreateDescriptorSetLayout(device, &setinfo, nullptr, &_multiTextureSetLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create descriptor set layout");
    }
    _mainDeletionQueue.push_function([=](){vkDestroyDescriptorSetLayout(device, _multiTextureSetLayout, nullptr);});

    VkDescriptorSetLayoutBinding skyboxSetBinding = vkStructs::descriptorset_layout_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    bindings.clear();
    bindings =  {skyboxSetBinding};
    setinfo = vkStructs::descriptorset_layout_create_info(bindings);
    if(vkCreateDescriptorSetLayout(device, &setinfo, nullptr, &_skyboxSetLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create skybox set layout");
    }
    _mainDeletionQueue.push_function([=](){vkDestroyDescriptorSetLayout(device, _skyboxSetLayout, nullptr);});
}

void VulkanEngine::init_pipelines(){

    //load and create all shader stages
    auto default_lit_vert_c = init_query::readFile("shaders/default_lit_vert.spv"); 
    auto default_lit_frag_c = init_query::readFile("shaders/default_lit_frag.spv"); //ths shader shouldnt be textured and should have a descriptor without the sampler
    auto unlit_frag_c = init_query::readFile("shaders/unlit_frag.spv"); //ths shader shouldnt be textured and should have a descriptor without the sampler
    auto textured_lit_frag_c = init_query::readFile("shaders/textured_lit_frag.spv");

    auto skybox_vert_c = init_query::readFile("shaders/skybox_vert.spv"); //ths shader shouldnt be textured and should have a descriptor without the sampler
    auto skybox_frag_c = init_query::readFile("shaders/skybox_frag.spv"); //ths shader shouldnt be textured and should have a descriptor without the sampler

    auto default_lit_vert_m = createShaderModule(default_lit_vert_c);
    auto default_lit_frag_m = createShaderModule(default_lit_frag_c);
    auto unlit_frag_m = createShaderModule(unlit_frag_c);
    auto textured_lit_frag_m = createShaderModule(textured_lit_frag_c);

    auto skybox_vert_m = createShaderModule(skybox_vert_c);
    auto skybox_frag_m = createShaderModule(skybox_frag_c);

    //begin default mesh pipeline creation (lit untextured mesh)
	PipelineBuilder pipelineBuilder;
    VkDescriptorSetLayout defaultSetLayouts[] = { _globalSetLayout, _objectSetLayout, _materialSetLayout, _lightingSetLayout };
    VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkStructs::pipeline_layout_create_info(4, defaultSetLayouts);
    
	//setup push constants
	VkPushConstantRange push_constant;
	//this push constant range starts at the beginning
	push_constant.offset = 0;
	//this push constant range takes up the size of a MeshPushConstants struct
	push_constant.size = sizeof(PushConstants);
	//this push constant range is accessible only in the vertex shader
	push_constant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	mesh_pipeline_layout_info.pPushConstantRanges = &push_constant;
	mesh_pipeline_layout_info.pushConstantRangeCount = 1;
    
    //create the layout
    VkPipelineLayout meshPipelineLayout;
    if(vkCreatePipelineLayout(device, &mesh_pipeline_layout_info, nullptr, &meshPipelineLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create pipeline layout");
    }
    _swapDeletionQueue.push_function([=](){vkDestroyPipelineLayout(device, meshPipelineLayout, nullptr);});

    //general setup of the pipeline
    pipelineBuilder.viewport = vkStructs::viewport(0.0f, 0.0f, (float) swapChainExtent.width, (float) swapChainExtent.height, 0.0f, 1.0f);
    pipelineBuilder.scissor = vkStructs::scissor(0,0,swapChainExtent);
    VertexInputDescription vertexInputDescription = Vertex::get_vertex_description();
    pipelineBuilder.vertexInputInfo = vkStructs::pipeline_vertex_input_create_info(vertexInputDescription);
    pipelineBuilder.inputAssembly = vkStructs::pipeline_input_assembly_state_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
    pipelineBuilder.rasterizer = vkStructs::pipeline_rasterization_state_create_info(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    pipelineBuilder.multisampling = vkStructs::pipeline_msaa_state_create_info(msaaSamples);
    pipelineBuilder.depthStencil = vkStructs::pipeline_depth_stencil_state_create_info(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);//VK_COMPARE_OP_LESS
    pipelineBuilder.colourBlendAttachment = vkStructs::pipeline_colour_blend_attachment(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
    pipelineBuilder.pipelineLayout = meshPipelineLayout;

    //add required shaders for this stage
    pipelineBuilder.shaderStages.clear();
    pipelineBuilder.shaderStages.push_back(vkStructs::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, default_lit_vert_m, nullptr));
	pipelineBuilder.shaderStages.push_back(vkStructs::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, default_lit_frag_m, nullptr));

    VkPipeline meshPipeline = pipelineBuilder.build_pipeline(device, renderPass);
    _swapDeletionQueue.push_function([=](){vkDestroyPipeline(device, meshPipeline, nullptr);});

    create_material(meshPipeline, meshPipelineLayout, "defaultmesh", 0);

    //that was the defaultmesh pipeline created, which has no sampler used in shader but is still lit 

    //now we will make an unlit pipeline with no texture sampler
    pipelineBuilder.shaderStages.clear();
    pipelineBuilder.shaderStages.push_back(vkStructs::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, default_lit_vert_m, nullptr));
	pipelineBuilder.shaderStages.push_back(vkStructs::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, unlit_frag_m, nullptr));

    VkPipeline unlitMeshPipeline = pipelineBuilder.build_pipeline(device, renderPass);
    _swapDeletionQueue.push_function([=](){vkDestroyPipeline(device, unlitMeshPipeline, nullptr);});

    create_material(unlitMeshPipeline, meshPipelineLayout, "unlitmesh", 1);
    
    //
    //create pipeline layout for the textured mesh, which has 3 descriptor sets
	//we start from  the normal mesh layout
    VkPipelineLayoutCreateInfo textured_pipeline_layout_info = mesh_pipeline_layout_info;
		
    VkDescriptorSetLayout texturedSetLayouts[] = {_globalSetLayout, _objectSetLayout, _materialSetLayout, _lightingSetLayout, _multiTextureSetLayout}; //come back to this, should have _materialSetLayout in here as well

	textured_pipeline_layout_info.setLayoutCount = 5;
	textured_pipeline_layout_info.pSetLayouts = texturedSetLayouts;

	VkPipelineLayout texturedPipeLayout;

    if(vkCreatePipelineLayout(device, &textured_pipeline_layout_info, nullptr, &texturedPipeLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create texture pipeline layout");
    }
    _swapDeletionQueue.push_function([=](){vkDestroyPipelineLayout(device, texturedPipeLayout, nullptr);});

    //now for the textured pipeline
    pipelineBuilder.shaderStages.clear();
	pipelineBuilder.shaderStages.push_back(vkStructs::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, default_lit_vert_m, nullptr));
	pipelineBuilder.shaderStages.push_back(vkStructs::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, textured_lit_frag_m, nullptr));

    pipelineBuilder.pipelineLayout = texturedPipeLayout;

	VkPipeline texPipeline = pipelineBuilder.build_pipeline(device, renderPass);
    _swapDeletionQueue.push_function([=](){vkDestroyPipeline(device, texPipeline, nullptr);});

    //use this texturedpipeline to create multiple textures with a name
	create_material(texPipeline, texturedPipeLayout, "texturedmesh1", 2);
    create_material(texPipeline, texturedPipeLayout, "texturedmesh2", 3);

    //Skybox
    VkDescriptorSetLayout skyboxSetLayouts[] = {_globalSetLayout, _objectSetLayout, _skyboxSetLayout};
    VkPipelineLayoutCreateInfo skybox_pipeline_layout_info = vkStructs::pipeline_layout_create_info(3, skyboxSetLayouts);

    if(vkCreatePipelineLayout(device, &skybox_pipeline_layout_info, nullptr, &_skyboxPipelineLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create skybox pipeline layout");
    }
    _swapDeletionQueue.push_function([=](){vkDestroyPipelineLayout(device, _skyboxPipelineLayout, nullptr);});


     //now for the skybox pipeline
    pipelineBuilder.shaderStages.clear();
	pipelineBuilder.shaderStages.push_back(vkStructs::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, skybox_vert_m, nullptr));
	pipelineBuilder.shaderStages.push_back(vkStructs::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, skybox_frag_m, nullptr));

    pipelineBuilder.rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT; //because we are drawing on the inside of the cube

    pipelineBuilder.pipelineLayout = _skyboxPipelineLayout;

	skyboxPipeline = pipelineBuilder.build_pipeline(device, renderPass);
    _swapDeletionQueue.push_function([=](){vkDestroyPipeline(device, skyboxPipeline, nullptr);});

    
    //because these wrappers just deliver the code they can be local variables and destroyed at the end of the function
    vkDestroyShaderModule(device, default_lit_vert_m, nullptr);
    vkDestroyShaderModule(device, default_lit_frag_m, nullptr);
    vkDestroyShaderModule(device, unlit_frag_m, nullptr);
    vkDestroyShaderModule(device, textured_lit_frag_m, nullptr);
    vkDestroyShaderModule(device, skybox_frag_m, nullptr);
    vkDestroyShaderModule(device, skybox_vert_m, nullptr);
}

//before we can pass shader code to the pipeline we have to wrap it in a VkShaderModule object
//lets create a helper function to do that
VkShaderModule VulkanEngine::createShaderModule(const std::vector<char>& code){
    VkShaderModuleCreateInfo createInfo = vkStructs::shader_module_create_info(code);
    VkShaderModule shaderModule;
    if(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS){
        throw std::runtime_error("Failed to create shader module");
    }
    return shaderModule;
}   

//Attachments specified during render pass creation are bound by wrapping them into a VkFramebuffer object
//A framebuffer object references all the VkImageView objects that represent the attachments. We are only using one, the color attachment
//However the image we have to use for the attachment depends on which image the swapchain returns when we retrieve one for presentation
//that means we have to create a frambuffer for all the images in the swap chain and the one that corresponds to the retrieved image at draw time
// so we create a vector class member called swapChainFramebuffers, and then create the objects here
void VulkanEngine::createFramebuffers(){
    //start by resizing the vector to hold all the framebuffers
    swapChainFramebuffers.resize(swapChainImageViews.size());

    //then iterative through imageviews and create the framebuffers for each
    for(size_t i = 0; i < swapChainImageViews.size(); i++){
        std::vector<VkImageView> attachments = {colourImageView, depthImageView, swapChainImageViews[i]};

        VkFramebufferCreateInfo framebufferInfo = vkStructs::framebuffer_create_info(renderPass, attachments, swapChainExtent, 1);

        if(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS){
            throw std::runtime_error("Failed to create framebuffer");
        }
        _swapDeletionQueue.push_function([=](){vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);});
    }
}

//Command in Vukan, like drawing operations and memory transfers, are not executed directly with function calls. 
//You have to record all the ops you want to perform in command buffer objects. The advantage is all the hard work of setting up the drawing
//commands can be done in advance and in multiple threads. After that you just have to tell Vulkan to execute the commands in the main loop
//Before we create command buffers we need a command pool. Command pools manage the memory that is used to store the buffers and 
//command buffers are alocated to them. Lets create a class member to store our VkCommandPool commandPool
void VulkanEngine::createCommandPools(){
    //Command buffers are execued by submitting them on one of the device queues, liek graphics or presentation queues we retrieved.
    //Each command pool can only allocate command buffers that are submitted on a single type of queue.
    VkCommandPoolCreateInfo poolInfo = vkStructs::command_pool_create_info(queueFamilyIndicesStruct.graphicsFamily.value());

    for (int i = 0; i < _frames.size(); i++) {
        if(vkCreateCommandPool(device, &poolInfo, nullptr, &_frames[i]._commandPool) != VK_SUCCESS){
            throw std::runtime_error("Failed to create main command pool");
        }        
        _mainDeletionQueue.push_function([=]() {
		    vkDestroyCommandPool(device, _frames[i]._commandPool, nullptr);
	    });
    }

    if(vkCreateCommandPool(device, &poolInfo, nullptr, &transferCommandPool) != VK_SUCCESS){
        throw std::runtime_error("Failed to create transfer command pool");
    }   

    _mainDeletionQueue.push_function([=]() {
		vkDestroyCommandPool(device, transferCommandPool, nullptr);
	});

    VkCommandPoolCreateInfo uploadCommandPoolInfo = vkStructs::command_pool_create_info(queueFamilyIndicesStruct.graphicsFamily.value());
	//create pool for upload context //unused?
	if(vkCreateCommandPool(device, &uploadCommandPoolInfo, nullptr, &_uploadContext._commandPool)!= VK_SUCCESS){
        throw std::runtime_error("Failed to create upload command pool");
    }
    _mainDeletionQueue.push_function([=]() {
		vkDestroyCommandPool(device, _uploadContext._commandPool, nullptr); 
	});

    //we also want to create a transient command pool on the transfer family queue
    //so we submit another after changing the releveant values and reusing the VkCommandPoolCreateInfo struct above
    poolInfo.queueFamilyIndex = queueFamilyIndicesStruct.transferFamily.value(); //we are submitting transfer commands so we use transfer queue family
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT; //indicate its transient
    if(vkCreateCommandPool(device, &poolInfo, nullptr, &transientCommandPool) != VK_SUCCESS){
        throw std::runtime_error("Failed to create transient command pool");
    }
    _mainDeletionQueue.push_function([=]() {
		vkDestroyCommandPool(device, transientCommandPool, nullptr); 
	});
}

//we will create colour resources here, this is used for MSAA and creates a single colour image that can be drawn to for calculating msaa 
//samples
void VulkanEngine::createColourResources(){
    VkFormat colorFormat = swapChainImageFormat;

    image_func::createImage(swapChainExtent.width, swapChainExtent.height, 1, 1, msaaSamples, (VkImageCreateFlagBits)0, colorFormat, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, colourImage,
        colourImageAllocation);

    colourImageView = image_func::createImageView(colourImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1);
    
    _swapDeletionQueue.push_function([=](){vmaDestroyImage(allocator, colourImage, colourImageAllocation);});
    _swapDeletionQueue.push_function([=](){vkDestroyImageView(device, colourImageView, nullptr);});
}

//create depth image
void VulkanEngine::createDepthResources(){
    // we will go with VK_FORMAT_D32_SFLOAT but will add a findSupportedFormat() first to get something supported
    VkFormat depthFormat = init_query::findDepthFormat(physicalDevice);
    image_func::createImage(swapChainExtent.width, swapChainExtent.height, 1, 1, msaaSamples, (VkImageCreateFlagBits)0, depthFormat, VK_IMAGE_TILING_OPTIMAL, 
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageAllocation);

    depthImageView = image_func::createImageView(depthImage, VK_IMAGE_VIEW_TYPE_2D, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1);
    
    _swapDeletionQueue.push_function([=](){vmaDestroyImage(allocator, depthImage, depthImageAllocation);});
    _swapDeletionQueue.push_function([=](){vkDestroyImageView(device, depthImageView, nullptr);});
}

//creates texture images by loading them from file and 
void VulkanEngine::createTextureImages(){
    _loadedTextures.clear();
    for(TextureInfo info : TEXTURE_INFOS){
        Texture texture;
        image_func::loadTextureImage(*this, info.filePath.c_str(), texture.image, texture.alloc, texture.mipLevels);
        VkImageViewCreateInfo imageinfo = vkStructs::image_view_create_info(texture.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_ASPECT_COLOR_BIT, 0, texture.mipLevels, 0, 1);
        vkCreateImageView(device, &imageinfo, nullptr, &texture.imageView);  
        _loadedTextures[info.textureName] = texture;
        _mainDeletionQueue.push_function([=](){vmaDestroyImage(allocator, _loadedTextures[info.textureName].image, _loadedTextures[info.textureName].alloc);});
        _mainDeletionQueue.push_function([=](){vkDestroyImageView(device, _loadedTextures[info.textureName].imageView, nullptr);});
    }

    //load skybox textures
    char* textureData[6];
    
    int width{ 0 };
    int height{ 0 };
    int numberOfChannels{ 0 };
    
    int i{0};
    for(std::string path : SKYBOX_PATHS){
        image_func::simpleLoadTexture(path.c_str(), width, height, textureData[i]);
        i++;
    }
        
    //Calculate the image size and the layer size.
    const VkDeviceSize imageSize = width * height * 4 * 6; //4 since I always load my textures with an alpha channel, and multiply it by 6 because the image must have 6 layers.
    const VkDeviceSize layerSize = imageSize / 6; //This is just the size of each layer.
    
    //Staging buffer
    //we will now create a host visible staging buffer so that we can map memory and copy pixels to it
    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferAllocation;
    VulkanEngine::createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, stagingBufferAllocation);
    
    //map the memory
    char* dataMemory;
    vmaMapMemory(VulkanEngine::allocator, stagingBufferAllocation, (void**)&dataMemory);
    

    //Copy the data into the staging buffer.
    for (uint8_t i = 0; i < 6; ++i){    
        memcpy(dataMemory + (layerSize * i), textureData[i], static_cast<size_t>(layerSize));
    }
    vmaUnmapMemory(VulkanEngine::allocator, stagingBufferAllocation);

    
    //Copy the data into the staging buffer.
    for (uint8_t i = 0; i < 6; ++i){    
        image_func::simpleFreeTexture(textureData[i]);
    }

    image_func::createImage(width, height, 1, 6, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, skyboxImage, skyboxAllocation);

    //we have created the texture image, next step is to copy the staging buffer to the texture image. This involves 2 steps
    //we first transition the image to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    //note because the iamge was created with VK_IMAGE_LAYOUT_UNDEFINED, we specify that as the initial layout. We can do this because we dont care
    //about its contents before performing the copy operation
    image_func::transitionImageLayout(skyboxImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, 6);
    //then execute the buffer to image copy operation
    image_func::copyBufferToImage(stagingBuffer, skyboxImage, static_cast<uint32_t>(width), static_cast<uint32_t>(height), 6);

    image_func::transitionImageLayout(skyboxImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 6);

    vmaDestroyBuffer(VulkanEngine::allocator, stagingBuffer, stagingBufferAllocation);

    skyboxImageView = image_func::createImageView(skyboxImage, VK_IMAGE_VIEW_TYPE_CUBE, VK_FORMAT_R8G8B8A8_SRGB,  VK_IMAGE_ASPECT_COLOR_BIT, 1, 6);


    _mainDeletionQueue.push_function([=](){vmaDestroyImage(allocator, skyboxImage, skyboxAllocation);});
    _mainDeletionQueue.push_function([=](){vkDestroyImageView(device, skyboxImageView, nullptr);});
}

//this is where we will load the model data into vertices and indices member variables
void VulkanEngine::loadModels(){
    //because we are using an unnordered_map with a custom type, we need to define equality and hash functons in Vertex
    std::unordered_map<Vertex, uint32_t> uniqueVertices{}; //store unique vertices in here, and check for repeating verticesto push index

    for(ModelInfo info : MODEL_INFOS){
        glm::vec3 colour = glm::vec3(0,0,1);//temp colour code
        if(info.modelName == "sphere")//temp colour code
            colour = glm::vec3(1,1,0.5f);//temp colour code
        else
            colour = glm::vec3(0,0,1);//temp colour code
        populateVerticesIndices(info.filePath, uniqueVertices, colour);
        _meshes[info.modelName] = _loadedMeshes[_loadedMeshes.size()-1];
    }
    std::cout << "Vertices count for all " << allVertices.size() << "\n";
    std::cout << "Indices count for all " << allIndices.size() << "\n";
}

void VulkanEngine::populateVerticesIndices(std::string path, std::unordered_map<Vertex, uint32_t>& uniqueVertices, glm::vec3 baseColour){
    Mesh mesh;
    _loadedMeshes.push_back(mesh);
    int numMeshes = _loadedMeshes.size() - 1;
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if(!tinyobj::LoadObj(&attrib, &shapes, & materials, & warn, & err, path.c_str())){
        throw std::runtime_error(warn + err);
    }

    _loadedMeshes[numMeshes].id = numMeshes; //used in identify and indexing for draw calls into storage buffer
    _loadedMeshes[numMeshes].indexBase = allIndices.size(); //start of indices will be 0 for first object

    for(const auto& shape : shapes){
        for(const auto& index : shape.mesh.indices){
            Vertex vertex{};
            //the index variable is of type tinyob::index_t which coontains vertex_index, normal_index and texcoord_index members
            //we need to use these indexes to look up the actual vertex attributes in the attrib arrays
            vertex.pos = { 
                //because attrib vertices is an  array of float instead of a format we want like glm::vec3, we multiply the index by 3
                //ie skipping xyz per vertex index. 
                attrib.vertices[3 * index.vertex_index + 0], //offset 0 is X
                attrib.vertices[3 * index.vertex_index + 1], //offset 1 is Y
                attrib.vertices[3 * index.vertex_index + 2] //offset 2 is Z
            };

            vertex.texCoord = {
                //Similarly there are two texture coordinate components per entry ie, U and V
                attrib.texcoords[2 * index.texcoord_index + 0], //offset 0 is U
                //because OBJ format assumes a coordinate system where a vertical coordinate of 0 means the bottom of the image,
                1 - attrib.texcoords[2 * index.texcoord_index + 1] //offset 1 is V
            };

            //default vertex colour
            vertex.colour = baseColour;

            vertex.normal = {
                attrib.normals[3 * index.normal_index + 0],
                attrib.normals[3 * index.normal_index + 1],
                attrib.normals[3 * index.normal_index + 2]
                
            };

            //store unique vertices in an unnordered map
            if(uniqueVertices.count(vertex) == 0){ //if vertices stored in uniqueVertices matching vertex == 0 
                uniqueVertices[vertex] = static_cast<uint32_t>(allVertices.size()); //add the count of vertices to the map matching the vertex
                allVertices.push_back(vertex); //add the vertex to vertices
            }
            allIndices.push_back(uniqueVertices[vertex]);
        }
    }
    _loadedMeshes[numMeshes].indexCount = allIndices.size() - _loadedMeshes[numMeshes].indexBase; //how many indices in this model
}

//used to pass vertices and indices to WorldState for building collision meshes from objects directly (only used for asteroid mesh)
std::vector<Vertex>& VulkanEngine::get_allVertices(){
    return allVertices;
}
std::vector<uint32_t>& VulkanEngine::get_allIndices(){
    return allIndices;
}

//Buffer Creation
//Buffers in Vulkan are regions of memory used for storing arbitrary data that can be read by the graphics card. 
//They can be used to stor vertex data which we will do here. But can be used for other thigns which we will look at later.
//Unlike other Vulkan objects that we have used so far, buffers do not automatically allocate memory for themselves
//We have a lot of control but have to manage the memory
//because we are going to create multiple buffers we will seperate the buffer creation into a helper function
//parms, size of buffer in bytes, uage flags, propert flags, buffer and buffer memory pointers to store the buffer and memory addresses
void VulkanEngine::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage vmaUsage, VkBuffer& buffer, VmaAllocation& allocation){
    uint32_t queueFamilyIndices[] = {queueFamilyIndicesStruct.graphicsFamily.value(), queueFamilyIndicesStruct.transferFamily.value()};
    //both graphics and transfer queue families involved so count 2 and concurrent sharing for now
    VkBufferCreateInfo bufferInfo = vkStructs::buffer_create_info(size, usage, VK_SHARING_MODE_CONCURRENT, 2, queueFamilyIndices);
    
    VmaAllocationCreateInfo vmaAllocInfo = vkStructs::vma_allocation_create_info(vmaUsage);

    vmaCreateBuffer(allocator, &bufferInfo, &vmaAllocInfo, &buffer, &allocation, nullptr);
}
    
void VulkanEngine::createVertexBuffer(){       //uploading vertex data to the gpu    
    VkDeviceSize bufferSize = sizeof(allVertices[0]) * allVertices.size(); // should add up all meshes loaded? not sure

    //Staging Buffer
    //The most optimal memory type for the graphics card to read from has a flag of VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT. This is usaully not
    //accessible by the CPU on dedicated graphics cards. To get our vertex data to this memory type we can create two Vertex Buffers, one Staging buffer
    //in CPU accessible memory to upload the data from the vertex array to, and the final vertex buffer in device lcoal memory. We then use a buffer
    //copy command to move the data from the staging buffer to the actual vertex buffer. This buffer copy command requires a queue family which supports
    //VK_QUEUE_TRANSFER_BIT. Luckily any family that supports VK_QUEUE_GRAPHICS_BIT or VK_QUEUE_COMPUTE_BIT already implicitly supports this
    //create our staging buffer
    VkBuffer stagingBuffer;      
    VmaAllocation stagingBufferAllocation;

    //create the staging buffer in CPU accessible memory
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, stagingBufferAllocation);

    //map data to it by copying the memory in, we must map the memory, copy, and then unmap
    void* mappedData;
    vmaMapMemory(allocator, stagingBufferAllocation, &mappedData);
    memcpy(mappedData, allVertices.data(), (size_t) bufferSize);
    vmaUnmapMemory(allocator, stagingBufferAllocation);

    //create the vertex buffer in GPU only memory
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT |  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
    VMA_MEMORY_USAGE_GPU_ONLY, vertexBuffer, vertexBufferAllocation);
    _mainDeletionQueue.push_function([=](){vmaDestroyBuffer(allocator, vertexBuffer, vertexBufferAllocation);});

    //now copy the contents of staging buffer into vertexBuffer
    copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

    //now the data is copied from staging to gpu memory we should cleanup
    vmaDestroyBuffer(allocator, stagingBuffer, stagingBufferAllocation);
}

//Index Buffer
//Used to store array of pointers to the vertices we want to draw
//Just like vertex data we need a VkBuffer for the GPU to be able to access them
//This function is almost identical to createVertexBuffer
//only differences are specifying indices and the size and data. and VK_BUFFER_USAGE_INDEX_BUFFER_BIT as the usage type
void VulkanEngine::createIndexBuffer(){
    VkDeviceSize bufferSize = sizeof(allIndices[0]) * allIndices.size();
    //VkDeviceSize bufferSize = sizeof(buildingMesh.indices[0]) * buildingMesh.indices.size();
    //VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();
    VkBuffer stagingBuffer;  //create our staging buffer      
    VmaAllocation stagingBufferAllocation;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, stagingBufferAllocation);
    void* mappedData;
    vmaMapMemory(allocator, stagingBufferAllocation, &mappedData);
    memcpy(mappedData, allIndices.data(), (size_t) bufferSize);
    //memcpy(mappedData, indices.data(), (size_t) bufferSize);
    vmaUnmapMemory(allocator, stagingBufferAllocation);
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT |  VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
    VMA_MEMORY_USAGE_GPU_ONLY, indexBuffer, indexBufferAllocation);
    _mainDeletionQueue.push_function([=](){vmaDestroyBuffer(allocator, indexBuffer, indexBufferAllocation);});
    copyBuffer(stagingBuffer, indexBuffer, bufferSize);
    //now the data is copied from staging to gpu memory we should cleanup
    vmaDestroyBuffer(allocator, stagingBuffer, stagingBufferAllocation);
}

//Uniform Buffer
//We will copy new data to the unifrom buffer every frame, so we dont need a staging buffer here, it would degrade performance
//we need multiple buffers because multiple frames may be in flight and we dont want to update the buffer in preperation for the next frame
//while a previous one is still reading it! We can eithe rhave a uniform buffer per frame or per swap image. Because we need to refer to the uniform
//buffer from the command buffer that we have per swao chain image, it makes sense to also have a uniform buffer per swap chain image
void VulkanEngine::createUniformBuffers(){
    for(size_t i = 0; i < swapChainImages.size(); i++){ //loop through each in swapChainImages

        //create a uniform buffer for each frame, will hold camera buffer information (matrices)
        createBuffer(sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, 
            _frames[i].cameraBuffer, _frames[i].cameraBufferAllocation);
        _swapDeletionQueue.push_function([=](){vmaDestroyBuffer(allocator, _frames[i].cameraBuffer, _frames[i].cameraBufferAllocation);});
        //create a large storage buffer for each frame, will hold all object matrices
        //storage buffers are similar to uniform buffers, except can be much larger and and writable by the shader
        //nice for compute shaders or large amounts of information
       
        createBuffer(sizeof(GPUObjectData) * MAX_OBJECTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, 
            _frames[i].objectBuffer, _frames[i].objectBufferAlloc);
        _swapDeletionQueue.push_function([=](){vmaDestroyBuffer(allocator, _frames[i].objectBuffer, _frames[i].objectBufferAlloc);});

        //create the materialBuffer one per swapchain frame
        createBuffer(sizeof(GPUMaterialData) * MATERIALS_COUNT, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, 
            _frames[i].materialBuffer, _frames[i].materialPropertiesBufferAlloc);
        _swapDeletionQueue.push_function([=](){vmaDestroyBuffer(allocator, _frames[i].materialBuffer, _frames[i].materialPropertiesBufferAlloc);});

        //create the point lights Buffer one per swapchain frame

        createBuffer(sizeof(GPUPointLightData) * MAX_LIGHTS, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
            _frames[i].pointLightParameterBuffer, _frames[i].pointLightParameterBufferAlloc);
        _swapDeletionQueue.push_function([=](){vmaDestroyBuffer(allocator, _frames[i].pointLightParameterBuffer, _frames[i].pointLightParameterBufferAlloc);});

        //create the spot lights Buffer one per swapchain frame
        createBuffer(sizeof(GPUSpotLightData) * MAX_LIGHTS, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
            _frames[i].spotLightParameterBuffer, _frames[i].spotLightParameterBufferAlloc);
        _swapDeletionQueue.push_function([=](){vmaDestroyBuffer(allocator, _frames[i].spotLightParameterBuffer, _frames[i].spotLightParameterBufferAlloc);});
    }

    const size_t sceneParamBufferSize = swapChainImages.size() * pad_uniform_buffer_size(sizeof(GPUSceneData));
    createBuffer(sceneParamBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
        _sceneParameterBuffer, _sceneParameterBufferAlloc);
    _swapDeletionQueue.push_function([=](){vmaDestroyBuffer(allocator, _sceneParameterBuffer, _sceneParameterBufferAlloc);});

    //assigning light param buffer of size swapChainImages.size() * padded GPULightData
   /* const size_t lightParamBufferSize = swapChainImages.size() * pad_uniform_buffer_size(sizeof(GPULightData));
    createBufferVMA(lightParamBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
        _lightParameterBuffer, _lightParameterBufferAlloc);
    _swapDeletionQueue.push_function([=](){vmaDestroyBuffer(allocator, _lightParameterBuffer, _lightParameterBufferAlloc);}); */
}

//helper function that begins single time commands, accepts a pool 
VkCommandBuffer VulkanEngine::beginSingleTimeCommands(VkCommandPool pool){
    //memory transfer operation are executed using command buffers, just like drawing commands. Therfor we must first allocate a temp command buffer.
    //You may wish to create a seperate command pool for these kinds of short lived buffers, because the implementation may be able to apply memory
    //allocation optimizations. You should use VK_COMMAND_POOL_CREATE_TRANSIENT_BIT flag during command pool generation in that case.
    VkCommandBufferAllocateInfo allocInfo = vkStructs::command_buffer_allocate_info(VK_COMMAND_BUFFER_LEVEL_PRIMARY, pool, 1);

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    //now we immediately start recording the command buffer
    //good practice to signal this is a one time submission.
    VkCommandBufferBeginInfo beginInfo = vkStructs::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

//helper function that ends single time commands, accepts a commandBuffer and a queue to submit to and wait on
void VulkanEngine::endSingleTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool pool, VkQueue queue) {
    vkEndCommandBuffer(commandBuffer);

    //now we execute the command buffer to complete the transfer
    VkSubmitInfo submitInfo = vkStructs::submit_info(&commandBuffer);

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE); //error here, we needed to submit to the transfer queue
    vkQueueWaitIdle(queue);

    //unlike draw command there are no events we need to wait on this time. We just want to execute on the buffers immediately.
    //we could use a fence and wait with vkWaitForFences, or simply wait for the transfer queue to become idle with vkQueueWaitIdle.
    //A fence would allow us to schedule multiple transfers simultaniously and wait for all of them to complete instead of executing one at a time.
    //Which may allow the driver to optimize.

    //dont forget to clean up the buffer used for the transfer operation
    //these are single time use commands so they will be on a transientcommand pool
    vkFreeCommandBuffers(device, pool, 1, &commandBuffer);
}

void VulkanEngine::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(transientCommandPool);
    VkBufferCopy copyRegion = vkStructs::buffer_copy(size);
    //contents of buffers are transferred using the vkCmdCopyBuffer command. Takes source and dest buffer as arguments and an array of regions to copy.
    //the regions are defined in VkBufferCopy structs and consist of a source buffer offset, destination buffer offset and size. 
    //note Can't specify VK_WHOLE_SIZE here unlike vkMapMemory
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    //as this command buffer should only contain the copy buffer command, we end it immediately after
    endSingleTimeCommands(commandBuffer, transientCommandPool, transferQueue); //in a copy buffer operation we want transferQueue
}

//Descriptor pool and sets
//descriptor layout from before describes the type of descriptors that can be bound
//now we create a descriptor set for each VkBuffer resource to binf it to the uniform buffer descriptor
//Descriptor sets can’t be created directly, they must be allocated from a pool like command buffers
//The equivalent for descriptor sets is unsurprisingly called a descriptor pool
void VulkanEngine::createDescriptorPool(){
    //create a descriptor pool that will hold 10 uniform buffers
	std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 20},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10}
    };
    VkDescriptorPoolCreateInfo poolInfo = vkStructs::descriptor_pool_create_info(poolSizes, 30);
    //now we can create the descriptor pool and store in the member variable descriptorPool
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool");
    }
    _swapDeletionQueue.push_function([=](){vkDestroyDescriptorPool(device, descriptorPool, nullptr);});
}


//Descriptor Set
//described with a VkDescriptorSetAllocateInfo struct. You need to specify the descriptor pool to allocate from, the number of
//descriptor sets to allocate, and the descriptor layout to base them on
void VulkanEngine::createDescriptorSets(){//do we really need one per swapchain image??????????????????
    for(size_t i = 0; i < swapChainImages.size(); i++){
        //descriptors that refer to buffers, like our uniform buffer descriptor, are configured with VkDescriptorBufferInfo
        //it specifies the buffer and region within it that contains the data for the descriptor
        
        //allocate globalDescriptor set for each frame
		VkDescriptorSetAllocateInfo globalSetAllocInfo = vkStructs::descriptorset_allocate_info(descriptorPool, &_globalSetLayout);
		if(vkAllocateDescriptorSets(device, &globalSetAllocInfo, &_frames[i].globalDescriptor) != VK_SUCCESS){
            throw std::runtime_error("Failed to allocate memory for globalDescriptor set");
        }

        //allocate a storage object descriptor set for each frame
        VkDescriptorSetAllocateInfo objectSetAllocInfo = vkStructs::descriptorset_allocate_info(descriptorPool, &_objectSetLayout);
		if(vkAllocateDescriptorSets(device, &objectSetAllocInfo, &_frames[i].objectDescriptor) != VK_SUCCESS){
            throw std::runtime_error("Failed to allocate memory for object descriptor set");
        }
        //allocate a uniform material descriptor set for each frame
        VkDescriptorSetAllocateInfo materialSetAllocInfo = vkStructs::descriptorset_allocate_info(descriptorPool, &_materialSetLayout);
		if(vkAllocateDescriptorSets(device, &materialSetAllocInfo, &_frames[i].materialSet) != VK_SUCCESS){
            throw std::runtime_error("Failed to allocate memory for material decriptor set");
        }
        //allocate a uniform lighting descriptor set for each frame
        VkDescriptorSetAllocateInfo lightingSetAllocInfo = vkStructs::descriptorset_allocate_info(descriptorPool, &_lightingSetLayout);
		if(vkAllocateDescriptorSets(device, &lightingSetAllocInfo, &_frames[i].lightSet) != VK_SUCCESS){
            throw std::runtime_error("Failed to allocate memory for lighting descriptor set");
        }
        std::array<VkWriteDescriptorSet, 6> setWrite{};

        //information about the buffer we want to point at in the descriptor
		VkDescriptorBufferInfo cameraInfo;
		cameraInfo.buffer = _frames[i].cameraBuffer; //it will be the camera buffer
		cameraInfo.offset = 0; //at 0 offset
		cameraInfo.range = sizeof(GPUCameraData); //of the size of a camera data struct
        //binding camera uniform buffer to 0
        setWrite[0] = vkStructs::write_descriptorset(0, _frames[i].globalDescriptor, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &cameraInfo);

        VkDescriptorBufferInfo sceneInfo;
		sceneInfo.buffer = _sceneParameterBuffer;
		sceneInfo.offset = 0; //we are using dynamic uniform buffer now so we dont hardcode the offset, 
        //instead we tell it the offset when binding the descriptorset at drawFrame (when we need to rebind it) hence dynamic
        //and they will be reading from the same buffer
		sceneInfo.range = sizeof(GPUSceneData);
        //binding scene uniform buffer to 1, we are using dynamic offsets so set the flag
        setWrite[1] = vkStructs::write_descriptorset(1, _frames[i].globalDescriptor, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, &sceneInfo);

        //object buffer info describes the location and the size
        VkDescriptorBufferInfo objectBufferInfo;
		objectBufferInfo.buffer = _frames[i].objectBuffer;
		objectBufferInfo.offset = 0;
		objectBufferInfo.range = sizeof(GPUObjectData) * MAX_OBJECTS;
        //notice we bind to 0 as this is part of a seperate descriptor set
        setWrite[2] = vkStructs::write_descriptorset(0, _frames[i].objectDescriptor, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &objectBufferInfo);

        //object buffer info describes the location and the size
        VkDescriptorBufferInfo materialBufferInfo;
		materialBufferInfo.buffer = _frames[i].materialBuffer;
		materialBufferInfo.offset = 0;
		materialBufferInfo.range = sizeof(GPUMaterialData) * MATERIALS_COUNT;
        //notice we bind to 0 as this is part of a seperate descriptor set
        setWrite[3] = vkStructs::write_descriptorset(0, _frames[i].materialSet, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &materialBufferInfo);

        //object buffer info describes the location and the size
        VkDescriptorBufferInfo pointlightsBufferInfo;
		pointlightsBufferInfo.buffer = _frames[i].pointLightParameterBuffer;
		pointlightsBufferInfo.offset = 0;
		pointlightsBufferInfo.range = sizeof(GPUPointLightData) * MAX_LIGHTS;
        //notice we bind to 0 as this is part of a seperate descriptor set
        setWrite[4] = vkStructs::write_descriptorset(0, _frames[i].lightSet, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &pointlightsBufferInfo);

        //object buffer info describes the location and the size
        VkDescriptorBufferInfo spotLightsBufferInfo;
		spotLightsBufferInfo.buffer = _frames[i].spotLightParameterBuffer;
		spotLightsBufferInfo.offset = 0;
		spotLightsBufferInfo.range = sizeof(GPUPointLightData) * MAX_LIGHTS;
        //notice we bind to 0 as this is part of a seperate descriptor set
        setWrite[5] = vkStructs::write_descriptorset(1, _frames[i].lightSet, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &spotLightsBufferInfo);

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(setWrite.size()), setWrite.data(), 0, nullptr);    
    }

     //allocate a skybox descriptor set for each frame
    VkDescriptorSetAllocateInfo skyboxSetAllocInfo = vkStructs::descriptorset_allocate_info(descriptorPool, &_skyboxSetLayout);
    if(vkAllocateDescriptorSets(device, &skyboxSetAllocInfo, &skyboxSet) != VK_SUCCESS){
        throw std::runtime_error("Failed to allocate memory for skybox descriptor set");
    }
}

//void VulkanEngine::

//Command buffer allocation
//Because one of the drawing commands involves binding the right VkFramebuffer, we will need to record a command buffer for every image in the swap chain
void VulkanEngine::createCommandBuffers(){
    clearValues.resize(2);
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};

    for (size_t i = 0; i < _frames.size(); i++){
		//allocate the default command buffer that we will use for rendering
		VkCommandBufferAllocateInfo cmdAllocInfo = vkStructs::command_buffer_allocate_info(VK_COMMAND_BUFFER_LEVEL_PRIMARY, _frames[i]._commandPool, 1);

		if(vkAllocateCommandBuffers(device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer) != VK_SUCCESS){
            throw std::runtime_error("Unable to allocate command buffer");
        }
        _swapDeletionQueue.push_function([=](){vkFreeCommandBuffers(device, _frames[i]._commandPool, 1, &_frames[i]._mainCommandBuffer);});
    }
}

//create VkSemaphores we need to use a struct, only the sType argument is required at this time
void VulkanEngine::createSyncObjects(){
    //this is to track for each swap chain image if a frame in flight is currently accessing it, 
    //we initialise as null because the list is empty to begin with as we dont have any frames in flight
    imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE); 
    
    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; //with a fence we must start it as signalled complete, otherwise it appears locked unless it can be reset

	for (int i = 0; i <_frames.size(); i++) {     
        if(vkCreateFence(device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence) != VK_SUCCESS){
            throw std::runtime_error("Failed to create fence");
        }
        _mainDeletionQueue.push_function([=](){vkDestroyFence(device, _frames[i]._renderFence, nullptr);});

        if(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &_frames[i]._presentSemaphore) != VK_SUCCESS){
            throw std::runtime_error("Failed to create semaphore");
        }
        _mainDeletionQueue.push_function([=](){vkDestroySemaphore(device, _frames[i]._presentSemaphore, nullptr);});
        if(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore) != VK_SUCCESS){
            throw std::runtime_error("Failed to create semaphore");
        }
        _mainDeletionQueue.push_function([=](){vkDestroySemaphore(device, _frames[i]._renderSemaphore, nullptr);});
	}

    VkFenceCreateInfo uploadFenceCreateInfo{};
    uploadFenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    //uploadFenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; //with this fence we wont set signal complete because we dont want to wait on it before submit

    if(vkCreateFence(device, &uploadFenceCreateInfo, nullptr, &_uploadContext._uploadFence) != VK_SUCCESS){
        throw std::runtime_error("Failed to create upload Fence");
    }		
    _mainDeletionQueue.push_function([=](){vkDestroyFence(device, _uploadContext._uploadFence, nullptr);});
}

//Swap Chain Recreation
//if the window surface changes (eg it is resized) then the swap chain becomes incompatible with it
//we have to catch these events and then the swap chain must be recreated. Here we recreate the chain
void VulkanEngine::recreateSwapChain(){
    int width = 0, height = 0;
    while(width == 0 || height == 0){ //handles minimizing, just pause if minimized
        glfwGetFramebufferSize(window, &width, &height); //keep checking the size
        glfwWaitEvents(); //between waiting for any glfw window events
    }
    vkDeviceWaitIdle(device); //first we wait until the resources are not in use
    cleanupSwapChain(); //clean old swapchain first
    createSwapChain(); //then create the swap chain
    createSwapChainImageViews(); //image views then need to be recreated because they are based directly on the swap chain images
    createColourResources();
    createDepthResources();
    createUniformBuffers(); //recreate our uniform buffers
    createDescriptorPool(); //recreate desc pool
    createDescriptorSets(); //recreate the desc sets 
    createRenderPass();  //render pass needs to be recreated because it depends on the format of the swap chain images
    createFramebuffers(); //framebuffers depend directly on swap chain images
    init_pipelines(); //viewport and scissor rectangle size is specified during graphics pipeline creation, so the pipeline needs rebuilt    
    createCommandBuffers(); //cammand buffers depend directly on swap chain images
    
    p_uiHandler->initUI(&device, &physicalDevice, &instance, queueFamilyIndicesStruct.graphicsFamily.value(), &graphicsQueue, &descriptorPool,
                (uint32_t)swapChainImages.size(), &swapChainImageFormat, &transferCommandPool, &swapChainExtent, &swapChainImageViews);

    //probs then need to link descriptors etc back to the materials?
    
    //init_scene();
    //mapMaterialDataToGPU();

    //ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount); //do i need to resize imgui? probably
}

Material* VulkanEngine::create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name, int id){
    if(id >= MATERIALS_COUNT){
        throw std::runtime_error("Materials id assigned higher value than material properties array in create_material");
    }
	Material mat;
	mat.pipeline = pipeline;
	mat.pipelineLayout = layout;
    mat.propertiesId = id;
	_materials[name] = mat;
	return &_materials[name];
}
Material* VulkanEngine::get_material(const std::string& name){
	//search for the object, and return nullpointer if not found
	auto it = _materials.find(name);
	if (it == _materials.end()) {
		return nullptr;
	}
	else {
		return &(*it).second;
	}
}
Mesh* VulkanEngine::get_mesh(const std::string& name){
	auto it = _meshes.find(name);
	if (it == _meshes.end()) {
		return nullptr;
	}
	else {
		return &(*it).second;
	}
}

//because we need to free swapChain resources before recreating it, we should have a seperate function to andle the cleanup that can be called from recreateSwapChain()
void VulkanEngine::cleanupSwapChain(){
    _swapDeletionQueue.flush();
}

size_t VulkanEngine::pad_uniform_buffer_size(size_t originalSize){
	// Calculate required alignment based on minimum device offset alignment
	size_t minUboAlignment = _gpuProperties.limits.minUniformBufferOffsetAlignment;
	size_t alignedSize = originalSize;
	if (minUboAlignment > 0) {
		alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
	}
	return alignedSize;
}

VulkanEngine::RenderStats VulkanEngine::getRenderStats(){
    return renderStats;
}
