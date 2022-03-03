//#define VMA_DEBUG_LOG(format, ...) do { \
//       printf(format); \
//       printf("\n"); \
//  } while(false)
#include "vk_renderer.h"
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <optional>
#include <set>
#include <cstdint>
#include <algorithm>
#include <array>
#define GLM_FORCE_RADIANS //makes sure GLM uses radians to avoid confusion
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES //forces GLM to use a version of vec2 and mat4 that have the correct alignment requirements for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE //forces GLM to use depth range of 0 to 1, instead of -1 to 1 as in OpenGL
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL //using glm to hash Vertex attributes using a template specialisation of std::hash<T>
#include <glm/gtx/hash.hpp> //used in unnordered_map to compare vertices
#include <glm/gtc/matrix_transform.hpp> //view transformations like glm::lookAt, and projection transformations like glm::perspective glm::rotate
#define TINYOBJLOADER_IMPLEMENTATION //same as with STD, define in one source file to avoid linker errors
#include <tiny_obj_loader.h> //we will use tiny obj loader to load our meshes from file (could use fast_obj_loader.h if this is too slow)
#include "vk_structures.h"
#include "vk_images.h"
#include "world_camera.h"
#include "obj_render.h"
#include <exception>
#include "vk_pipeline.h"
#include "vk_init_queries.h"

Vk::Renderer::Renderer(GLFWwindow* windowptr, Mediator& mediator): RendererBase(windowptr, mediator){}

void Vk::Renderer::init(){
    Vk::RendererBase::init();
}

void Vk::Renderer::resetScene(){
    sceneShutdownRequest = true;
}

void Vk::Renderer::setRenderablesPointer(std::vector<std::shared_ptr<RenderObject>>* renderableObjects){
    p_renderables = renderableObjects;

    //this is temporarily here
    for(auto iter : _materials){
        _materialParameters[iter.second.propertiesId].diffuse = iter.second.diffuse;
        _materialParameters[iter.second.propertiesId].specular = iter.second.specular;
        _materialParameters[iter.second.propertiesId].extra = iter.second.extra;
    }
}

void Vk::Renderer::setLightPointers(WorldLightObject* sceneLight, std::vector<WorldPointLightObject>* pointLights, std::vector<WorldSpotLightObject>* spotLights){
    p_sceneLight = sceneLight;
    p_pointLights = pointLights;
    p_spotLights = spotLights;
}

void Vk::Renderer::allocateDescriptorSetForSkybox(){
    VkWriteDescriptorSet setWrite{};
    VkDescriptorImageInfo skyImageInfo;
    skyImageInfo.sampler = skyboxSampler;
    skyImageInfo.imageView = skyboxImageView; //this should exist before this call
    skyImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    setWrite = Vk::Structures::write_descriptorset(0, skyboxSet, 1, 
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &skyImageInfo);
    queueSubmitMutex.lock();
    vkUpdateDescriptorSets(device,  1, &setWrite, 0, nullptr);
    queueSubmitMutex.unlock();
}

void Vk::Renderer::allocateDescriptorSetForTexture(std::string materialName, std::string name){
    Material* material = getMaterial(materialName);
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

        std::vector<VkWriteDescriptorSet> setWrite;
        VkDescriptorImageInfo diffImageInfo;
        diffImageInfo.sampler = diffuseSampler;
        diffImageInfo.imageView = _loadedTextures[name+"_diff"].imageView;
        diffImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorImageInfo specImageInfo;
        specImageInfo.sampler = specularSampler;
        specImageInfo.imageView = _loadedTextures[name+"_spec"].imageView;
        specImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        //normal mapping should be included here, need normal map sampler

        setWrite.push_back(Vk::Structures::write_descriptorset(0, material->_multiTextureSets[0], 1, 
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &diffImageInfo));

        setWrite.push_back(Vk::Structures::write_descriptorset(1, material->_multiTextureSets[0], 1, 
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &specImageInfo));

        std::cout<< materialName << " mat name\n";
         std::cout<< name << " name\n";

        queueSubmitMutex.lock();
        try{
            vkUpdateDescriptorSets(device,  setWrite.size(), setWrite.data(), 0, nullptr);
        }
        catch(std::exception& e){
            std::cout << "Failed to update descriptor sets for  Texture " + name + "\n";
            std::cout << e.what() << "\n";
        }
        queueSubmitMutex.unlock();
    }
}

void Vk::Renderer::updateObjectTranslations(){
    glm::mat4 scale;
    glm::mat4 translation;
    glm::mat4 rotation;

    for (int i = 0; i < p_renderables->size(); i++){
        scale = glm::scale(glm::mat4{ 1.0 }, p_renderables->at(i)->scale);
        translation = glm::translate(glm::mat4{ 1.0 }, p_renderables->at(i)->pos);
        rotation = p_renderables->at(i)->rot;
        p_renderables->at(i)->transformMatrix = translation * rotation * scale;
    }
}

void Vk::Renderer::setCameraData(CameraData* camData){
    p_cameraData = camData;
}

void Vk::Renderer::populateCameraData(GPUCameraData& camData){
    glm::vec3 camPos = p_cameraData->cameraPos;
    glm::mat4 view = glm::lookAt(camPos, camPos + p_cameraData->cameraFront, p_cameraData->cameraUp);
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float) swapChainExtent.height, 0.1f, 15000.0f);
    //GLM was originally designed for OpenGL, where the Y coordinate of the clip coordinates is inverted, so we simply flip it
    //view[1][1] *= -1; //<--- really trippy :)
    proj[1][1] *= -1;
    camData.projection = proj;
    camData.view = view;
	camData.viewproj = proj * view;
}

void Vk::Renderer::mapMaterialDataToGPU(){ //not sure i need 3 of these if they were going to be read only anyway, test at some point
    for(int i = 0; i < swapChainImages.size(); i++){
        //copy material data array into position, can i do this outside of the render loop as it will be static? lets try
        char* materialData;
        vmaMapMemory(allocator, _frames[i].materialPropertiesBufferAlloc , (void**)&materialData);
        memcpy(materialData, &_materialParameters, sizeof(_materialParameters));
        vmaUnmapMemory(allocator, _frames[i].materialPropertiesBufferAlloc); 
    }
}

void Vk::Renderer::mapLightingDataToGPU(){
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

    //now for the lightVP matrix uniform buffer
    for(int i = 0; i < swapChainImages.size(); i++){
        //copy current point light data array into buffer
        char* lightVPData;
        vmaMapMemory(allocator, _frames[i].lightVPBufferAlloc , (void**)&lightVPData);
        memcpy(lightVPData, &lightVPParameters, sizeof(lightVPParameters));
        vmaUnmapMemory(allocator, _frames[i].lightVPBufferAlloc); 
    }
}

//
void Vk::Renderer::updateSceneData(GPUCameraData& camData){
    glm::vec3 lightDir = glm::vec3(camData.view * glm::vec4(p_sceneLight->pos, 0));
    _sceneParameters.lightDirection = glm::vec4(lightDir, 0);
    _sceneParameters.lightAmbient = glm::vec4(p_sceneLight->ambient, 1);
    _sceneParameters.lightDiffuse = glm::vec4(p_sceneLight->diffuse, 1);
    _sceneParameters.lightSpecular = glm::vec4(p_sceneLight->specular, 1);
    // Compute the MVP matrix from the light's point of view, ortho projection, need to adjust values
    glm::mat4 projectionMatrix = glm::ortho<float>(-1000,1000,-1000,1000,-1000,1000);
    glm::mat4 viewMatrix = glm::lookAt(lightDir, glm::vec3(0,0,0), glm::vec3(0,1,0));
    glm::mat4 modelMatrix = glm::mat4(1.0);
    lightVPParameters[p_sceneLight->layer].viewproj = projectionMatrix * viewMatrix * modelMatrix;
    //this is used to render the scene from the lights POV to generate a shadow map
}

//point lights and spotlights
void Vk::Renderer::updateLightingData(GPUCameraData& camData){
    for(int i = 0; i < p_pointLights->size(); i++){
        _pointLightParameters[i].position = glm::vec3(camData.view * glm::vec4(p_pointLights->at(i).pos, 1));
        _pointLightParameters[i].diffuse = p_pointLights->at(i).diffuse;
        _pointLightParameters[i].ambient = p_pointLights->at(i).ambient;
        _pointLightParameters[i].specular = p_pointLights->at(i).specular;
        _pointLightParameters[i].attenuation = p_pointLights->at(i).attenuation;
        //not going to implement pointlight shadow maps.
        //When we do want to we will need to create a shadow cube map per light instead of a 2d texture (because point lights emit in all directions)
    }

    for(int i = 0; i < p_spotLights->size(); i++){
        _spotLightParameters[i].position = glm::vec3(camData.view * glm::vec4(p_spotLights->at(i).pos, 1));
        _spotLightParameters[i].diffuse = p_spotLights->at(i).diffuse;
        _spotLightParameters[i].ambient = p_spotLights->at(i).ambient;
        _spotLightParameters[i].specular = p_spotLights->at(i).specular;
        _spotLightParameters[i].attenuation = p_spotLights->at(i).attenuation;
        _spotLightParameters[i].direction = glm::vec3(camData.view * glm::vec4(p_spotLights->at(i).direction, 0));
        _spotLightParameters[i].cutoffs = p_spotLights->at(i).cutoffs;

        //mvpMatrix for shadowmap, perpective projection
        glm::mat4 clip = glm::mat4(1.0f, 0.0f, 0.0f, 0.0f,
                           0.0f,-1.0f, 0.0f, 0.0f,
                           0.0f, 0.0f, 0.5f, 0.0f,
                           0.0f, 0.0f, 0.5f, 1.0f);

        glm::mat4 projectionMatrix = clip * glm::perspective<float>(glm::radians(p_spotLights->at(i).cutoffAngles.y), 1.0f,  p_spotLights->at(i).cutoffAngles.x, p_spotLights->at(i).cutoffAngles.y);
        
        glm::mat4 viewMatrix = glm::lookAt(p_spotLights->at(i).pos, p_spotLights->at(i).pos - p_spotLights->at(i).direction, glm::vec3(0,1,0));
        glm::mat4 modelMatrix = glm::mat4(1.0);
        lightVPParameters[p_sceneLight->layer].viewproj = projectionMatrix * viewMatrix * modelMatrix;
    }
}

void Vk::Renderer::drawFrame(){
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

    //we have the imageIndex and are cleared for operations on it so it should be safe to record the command buffer here
    recordCommandBuffer_Objects(imageIndex);
    recordCommandBuffer_GUI(imageIndex);

    VkSemaphore waitSemaphores[] = {_frames[currentFrame]._presentSemaphore}; 
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}; 
    VkSemaphore signalSemaphores[] = {_frames[currentFrame]._renderSemaphore};

    //Submitting the command buffer
    //queue submission and synchronization is configured through VkSubmitInfo struct
    VkCommandBuffer buffers[] = {_frames[imageIndex]._mainCommandBuffer, guiCommandBuffers[imageIndex]};
    VkSubmitInfo submitInfo = Vk::Structures::submit_info(buffers, 2, waitSemaphores, 1,
        waitStages, signalSemaphores, 1);
    
    //move our reset fences here, its best to simply reset a fence right before its used
    vkResetFences(device, 1, &_frames[currentFrame]._renderFence); //unlike semaphores, fences must be manually reset

    //submit the queue
    queueSubmitMutex.lock();
    if(vkQueueSubmit(graphicsQueue, 1 , &submitInfo, _frames[currentFrame]._renderFence) != VK_SUCCESS) { //then fails here after texture flush
        throw std::runtime_error("Failed to submit draw command buffer");
    }
    
    //Presentation
    //The last step of drawing a frame is submitting the result back to the swapchain
    VkSwapchainKHR swapChains[] = {swapChain};
    VkPresentInfoKHR presentInfo = Vk::Structures::present_info(&_frames[currentFrame]._renderSemaphore, 1, swapChains, &imageIndex);

    //now we submit the request to present an image to the swapchain. We'll add error handling later, as failure here doesnt necessarily mean we should stop
    result = vkQueuePresentKHR(presentQueue, &presentInfo); //takes ages after texture flush for some reason

    queueSubmitMutex.unlock();

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
    calculateFrameRate(); //just to display in the UI

    //if user has quit the scene we set resetTextureImages to true
    //then flush textures and related buffers, unset the renderables pointer
    //this could be cleaner
    if(sceneShutdownRequest){
        sceneShutdownRequest = false;
        flushSceneBuffers();
        flushTextures();
        r_mediator.application_resetScene();
        p_renderables = NULL;
    }
}

void Vk::Renderer::recordCommandBuffer_Objects(int i){
    //have to reset this even if not recording it!
    vkResetCommandBuffer(_frames[i]._mainCommandBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT); 

    //must record even if scene isnt loaded to allow it to clear the background from the UI
    VkCommandBufferBeginInfo beginInfo = Vk::Structures::command_buffer_begin_info(0);
    if(vkBeginCommandBuffer(_frames[i]._mainCommandBuffer, &beginInfo) != VK_SUCCESS){
        throw std::runtime_error("Failed to begin recording command buffer");
    }

    //note that the order of clear values should be identical to the order of attachments
    VkRenderPassBeginInfo renderPassBeginInfo = Vk::Structures::render_pass_begin_info(renderPass, swapChainFramebuffers[i], {0, 0}, swapChainExtent, clearValues);

    vkCmdBeginRenderPass(_frames[i]._mainCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    if(r_mediator.application_getSceneLoaded())
    if(p_renderables != NULL && !p_renderables->empty()){   
        VkBuffer vertexBuffers[] = {vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(_frames[i]._mainCommandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(_frames[i]._mainCommandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        drawObjects(i);
    }
    
    //we we can end the render pass
    vkCmdEndRenderPass(_frames[i]._mainCommandBuffer);

    //and we have finished recording, we check for errors here
    if (vkEndCommandBuffer(_frames[i]._mainCommandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void Vk::Renderer::recordCommandBuffer_GUI(int i){
    //now for the UI render pass
    vkResetCommandBuffer(guiCommandBuffers[i], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT); 
    
    VkCommandBufferBeginInfo beginInfo = Vk::Structures::command_buffer_begin_info(0);
    if(vkBeginCommandBuffer(guiCommandBuffers[i], &beginInfo) != VK_SUCCESS){
        throw std::runtime_error("Failed to begin recording gui command buffer");
    }

    r_mediator.ui_drawUI(); //then we will draw UI

    //note that the order of clear values should be identical to the order of attachments 
    VkRenderPassBeginInfo renderPassBeginInfo = Vk::Structures::render_pass_begin_info(guiRenderPass, guiFramebuffers[i], {0, 0}, swapChainExtent, clearValues);
    vkCmdBeginRenderPass(guiCommandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Record Imgui Draw Data and draw funcs into command buffer
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), guiCommandBuffers[i]);

    //we we can end the render pass
    vkCmdEndRenderPass(guiCommandBuffers[i]);

     //and we have finished recording, we check for errrors here
    if (vkEndCommandBuffer(guiCommandBuffers[i]) != VK_SUCCESS) {
        throw std::runtime_error("failed to record gui command buffer!");
    }
}

void Vk::Renderer::drawObjects(int curFrame){
    ////fill a GPU camera data struct
	GPUCameraData camData;
    Vk::Renderer::populateCameraData(camData);
    Vk::Renderer::updateSceneData(camData);
    
    //and copy it to the buffer
	void* data;
	vmaMapMemory(allocator, _frames[curFrame].cameraBufferAllocation, &data);
	memcpy(data, &camData, sizeof(GPUCameraData));
	vmaUnmapMemory(allocator, _frames[curFrame].cameraBufferAllocation);

    //fetch latest lighting data
    Vk::Renderer::updateLightingData(camData);
    //map it to the GPU
    Vk::Renderer::mapLightingDataToGPU();

    //UPDATE TRANSFORM MATRICES OF ALL MODELS HERE
    Vk::Renderer::updateObjectTranslations();
    
    //mapObjectDataToGPU();
    
    //then do the objects data into the storage buffer
    void* objectData;
    vmaMapMemory(allocator, _frames[curFrame].objectBufferAlloc, &objectData);
    GPUObjectData* objectSSBO = (GPUObjectData*)objectData;
    //loop through all objects in the scene, need a better container than a vector
    for (int i = 0; i < p_renderables->size(); i++){
        objectSSBO[i].modelMatrix = p_renderables->at(i)->transformMatrix;
        //calculate the normal matrix, its done by inverse transpose of the model matrix * view matrix because we are working 
        //in view space in the shader. taking only the top 3x3
        //this is to account for rotation and scaling when using normals. should be done on cpu as its costly
        objectSSBO[i].normalMatrix = glm::transpose(glm::inverse(camData.view * p_renderables->at(i)->transformMatrix)); 
    }
    vmaUnmapMemory(allocator, _frames[curFrame].objectBufferAlloc);
  
	char* sceneData;
    vmaMapMemory(allocator, _sceneParameterBufferAlloc , (void**)&sceneData);
	int frameIndex = frameCounter % MAX_FRAMES_IN_FLIGHT;
	sceneData += pad_uniform_buffer_size(sizeof(GPUSceneData)) * frameIndex; 
	memcpy(sceneData, &_sceneParameters, sizeof(GPUSceneData));
	vmaUnmapMemory(allocator, _sceneParameterBufferAlloc); 

    Material* lastMaterial = nullptr;

    std::vector<int> renderObjectIds;
    
    if(ENABLE_DEBUG_DRAWING)
        //renderObjectIds = std::vector{1,2,3,4,5,6,7,8,9,10}; //8 9 10 are debug boxes
        renderObjectIds = std::vector{1,2,3,4,5,6,7,8,9,10}; //8 9 10 are debug boxes
    else
        renderObjectIds = std::vector{1,2,3,4,5,6,7};

    //temp code for drawing plane guides
    //if(ENABLE_PLANE_DRAWING)
    for(int c = 0; c < 10; c++){
        renderObjectIds.push_back(11+c);
    }
    
    for(const int i : renderObjectIds){
    //for (int i = 1; i < p_renderables->size(); i++){
        RenderObject* object = p_renderables->at(i).get();
        //only bind the pipeline if it doesn't match with the already bound one
		if (object->material != lastMaterial) {
            if(object->material == nullptr){throw std::runtime_error("object.material is a null reference in drawObjects()");} //remove if statement in release
			vkCmdBindPipeline(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object->material->pipeline);
			lastMaterial = object->material;

            //offset for our scene buffer
            uint32_t scene_uniform_offset = pad_uniform_buffer_size(sizeof(GPUSceneData)) * frameIndex;
            //bind the descriptor set when changing pipeline
            vkCmdBindDescriptorSets(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                object->material->pipelineLayout, 0, 1, &_frames[curFrame].globalDescriptor, 1, &scene_uniform_offset);

	        //object data descriptor
        	vkCmdBindDescriptorSets(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                object->material->pipelineLayout, 1, 1, &_frames[curFrame].objectDescriptor, 0, nullptr);

             //material descriptor, we are using a dynamic uniform buffer and referencing materials by their offset with propertiesId, which just stores the int relative to the material in _materials
        	vkCmdBindDescriptorSets(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                object->material->pipelineLayout, 2, 1, &_frames[curFrame].materialSet, 0, nullptr);
            //the "firstSet" param above is 2 because in init_pipelines its described in VkDescriptorSetLayout[] in index 2!
            
            vkCmdBindDescriptorSets(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                object->material->pipelineLayout, 3, 1, &_frames[curFrame].lightSet, 0, nullptr);

            if (object->material->_multiTextureSets.size() > 0) {
                vkCmdBindDescriptorSets(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                    object->material->pipelineLayout, 4, 1, &object->material->_multiTextureSets[0], 0, nullptr);
		    }
		}

        //add material property id for this object to push constant
		PushConstants constants;
		constants.matIndex = object->material->propertiesId;
        constants.numPointLights = p_pointLights->size();
        constants.numSpotLights = p_spotLights->size();

        //upload the mesh to the GPU via pushconstants
		vkCmdPushConstants(_frames[curFrame]._mainCommandBuffer, object->material->pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &constants);

        vkCmdDrawIndexed(_frames[curFrame]._mainCommandBuffer, _loadedMeshes[object->meshId].indexCount, 1, _loadedMeshes[object->meshId].indexBase, 0, i); //using i as index for storage buffer in shaders
    }

    //draw skybox
    uint32_t scene_uniform_offset = pad_uniform_buffer_size(sizeof(GPUSceneData)) * frameIndex;
    vkCmdBindDescriptorSets(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _skyboxPipelineLayout, 0, 1, &_frames[curFrame].globalDescriptor, 1, &scene_uniform_offset);
    vkCmdBindDescriptorSets(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _skyboxPipelineLayout, 1, 1, &_frames[curFrame].objectDescriptor, 0, nullptr);
    vkCmdBindDescriptorSets(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _skyboxPipelineLayout, 2, 1, &skyboxSet, 0, NULL);
	vkCmdBindPipeline(_frames[curFrame]._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline);
	vkCmdDrawIndexed(_frames[curFrame]._mainCommandBuffer, _loadedMeshes[p_renderables->at(0)->meshId].indexCount, 1, _loadedMeshes[p_renderables->at(0)->meshId].indexBase, 0, 0);
}

void Vk::Renderer::calculateFrameRate(){
    double currentFrameTime = glfwGetTime();
    if(currentFrameTime - previousFrameTime >= 1.0 ){ // If last update was more than 1 sec ago
        renderStats.framerate = 1000.0/double(frameCounter-previousFrameCount);
        renderStats.fps = frameCounter-previousFrameCount;
        previousFrameTime += 1.0;
        previousFrameCount = frameCounter;
    }
}

//creates texture images by loading them from file and 
void Vk::Renderer::createTextureImages(const std::vector<TextureInfo>& TEXTURE_INFOS, const std::vector<std::string>& SKYBOX_PATHS){
    _loadedTextures.clear();
    for(TextureInfo info : TEXTURE_INFOS){
        Texture texture;
        std::cout << info.filePath.c_str() << "\n";
        imageHelper->loadTextureImage(*this, info.filePath.c_str(), texture.image, texture.alloc, texture.mipLevels);
        VkImageViewCreateInfo imageinfo = Vk::Structures::image_view_create_info(texture.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_ASPECT_COLOR_BIT, 0, texture.mipLevels, 0, 1);
        vkCreateImageView(device, &imageinfo, nullptr, &texture.imageView);  
        _loadedTextures[info.textureName] = texture;
        _textureDeletionQueue.push_function([=](){vmaDestroyImage(allocator, _loadedTextures[info.textureName].image, _loadedTextures[info.textureName].alloc);});
        _textureDeletionQueue.push_function([=](){vkDestroyImageView(device, _loadedTextures[info.textureName].imageView, nullptr);});
    }

    //load skybox textures
    char* textureData[6];
    
    int width{ 0 };
    int height{ 0 };
    int numberOfChannels{ 0 };
    
    int i{0};
    for(std::string path : SKYBOX_PATHS){
        imageHelper->simpleLoadTexture(path.c_str(), width, height, textureData[i]);
        i++;
    }
        
    //Calculate the image size and the layer size.
    const VkDeviceSize imageSize = width * height * 4 * 6; //4 since I always load my textures with an alpha channel, and multiply it by 6 because the image must have 6 layers.
    const VkDeviceSize layerSize = imageSize / 6; //This is just the size of each layer.
    
    //Staging buffer
    //we will now create a host visible staging buffer so that we can map memory and copy pixels to it
    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferAllocation;
    createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, stagingBufferAllocation);
    
    //map the memory
    char* dataMemory;
    vmaMapMemory(allocator, stagingBufferAllocation, (void**)&dataMemory);
    
    //Copy the data into the staging buffer.
    for (uint8_t i = 0; i < 6; ++i){    
        memcpy(dataMemory + (layerSize * i), textureData[i], static_cast<size_t>(layerSize));
    }
    vmaUnmapMemory(allocator, stagingBufferAllocation);

    //Copy the data into the staging buffer.
    for (uint8_t i = 0; i < 6; ++i){    
        imageHelper->simpleFreeTexture(textureData[i]);
    }

    imageHelper->createImage(width, height, 1, 6, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, skyboxImage, skyboxAllocation, VMA_MEMORY_USAGE_GPU_ONLY);

    //we have created the texture image, next step is to copy the staging buffer to the texture image. This involves 2 steps
    //we first transition the image to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    //note because the iamge was created with VK_IMAGE_LAYOUT_UNDEFINED, we specify that as the initial layout. We can do this because we dont care
    //about its contents before performing the copy operation
    imageHelper->transitionImageLayout(skyboxImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, 6);
    //then execute the buffer to image copy operation
    imageHelper->copyBufferToImage(stagingBuffer, skyboxImage, static_cast<uint32_t>(width), static_cast<uint32_t>(height), 6);

    imageHelper->transitionImageLayout(skyboxImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 6);

    vmaDestroyBuffer(allocator, stagingBuffer, stagingBufferAllocation);

    skyboxImageView = imageHelper->createImageView(skyboxImage, VK_IMAGE_VIEW_TYPE_CUBE, VK_FORMAT_R8G8B8A8_SRGB,  VK_IMAGE_ASPECT_COLOR_BIT, 1, 6);

    _textureDeletionQueue.push_function([=](){vmaDestroyImage(allocator, skyboxImage, skyboxAllocation);});
    _textureDeletionQueue.push_function([=](){vkDestroyImageView(device, skyboxImageView, nullptr);});
}

//this is where we will load the model data into vertices and indices member variables
void Vk::Renderer::loadModels(const std::vector<ModelInfo>& MODEL_INFOS){
    //because we are using an unnordered_map with a custom type, we need to define equality and hash functons in Vertex
    std::unordered_map<Vertex, uint32_t> uniqueVertices{}; //store unique vertices in here, and check for repeating vertices to push index
    allIndices.clear();
    allVertices.clear();

    for(ModelInfo info : MODEL_INFOS){
        glm::vec3 colour = glm::vec3(0,0,1);//temp colour code
        if(info.modelName == "sphere")//temp colour code
            colour = glm::vec3(1,1,1);//temp colour code
        else if(info.modelName == "sphere1")
            colour = glm::vec3(0.2,1,1);//temp colour code
        else if(info.modelName == "sphere2")
            colour = glm::vec3(0.4,0.8,1);//temp colour code
        else if(info.modelName == "sphere3")
            colour = glm::vec3(0.6,0.6,1);//temp colour code
        else if(info.modelName == "sphere4")
            colour = glm::vec3(0.8,0.4,1);//temp colour code
        else if(info.modelName == "sphere5")
            colour = glm::vec3(1,0.2,1);//temp colour code
        else if(info.modelName == "sphere6")
            colour = glm::vec3(1,1,0.2);//temp colour code
        else if(info.modelName == "sphere7")
            colour = glm::vec3(1,1,0.4);//temp colour code
        else if(info.modelName == "sphere8")
            colour = glm::vec3(1,1,0.6);//temp colour code
        else if(info.modelName == "sphere9")
            colour = glm::vec3(1,1,0.8);//temp colour code
        else if(info.modelName == "sphere10")
            colour = glm::vec3(1,1,1);//temp colour code
        else
            colour = glm::vec3(0,0,1);//temp colour code
        populateVerticesIndices(info.filePath, uniqueVertices, colour);
        _meshes[info.modelName] = _loadedMeshes[_loadedMeshes.size()-1];
    }
    std::cout << "Vertices count for all " << allVertices.size() << "\n";
    std::cout << "Indices count for all " << allIndices.size() << "\n";

    createVertexBuffer(); //error here if load models later
    createIndexBuffer(); //error here if load models later
}

void Vk::Renderer::populateVerticesIndices(std::string path, std::unordered_map<Vertex, uint32_t>& uniqueVertices, glm::vec3 baseColour){
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
std::vector<Vertex>& Vk::Renderer::get_allVertices(){
    return allVertices;
}
std::vector<uint32_t>& Vk::Renderer::get_allIndices(){
    return allIndices;
}

Mesh* Vk::Renderer::getLoadedMesh(const std::string& name){
	auto it = _meshes.find(name);
	if (it == _meshes.end()) {
		return nullptr;
	}
	else {
		return &(*it).second;
	}
}


/*void Vk::Renderer::createShadowMapPipeline(){
    auto shadowmap_vert_c = Vk::Init::readFile("resources/shaders/shadowmap_vert.spv"); 
    auto shadowmap_vert_m = createShaderModule(shadowmap_vert_c);
    auto shadowmap_frag_test_c = Vk::Init::readFile("resources/shaders/shadowmap_frag_test.spv"); 
    auto shadowmap_frag_test_m = createShaderModule(shadowmap_frag_test_c);

    VkDescriptorSetLayout shadowmapSetLayouts[] = {lightVPSetLayout, _objectSetLayout};
    VkPipelineLayoutCreateInfo shadowmap_pipeline_layout_info = Vk::Structures::pipeline_layout_create_info(2, shadowmapSetLayouts);

    //create the layout
    shadowmapPipelineLayout;
    if(vkCreatePipelineLayout(device, &shadowmap_pipeline_layout_info, nullptr, &shadowmapPipelineLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create shadowmap pipeline layout");
    }
    _swapDeletionQueue.push_function([=](){vkDestroyPipelineLayout(device, shadowmapPipelineLayout, nullptr);});

    PipelineBuilder pipelineBuilder;

    //general setup of the pipeline
    pipelineBuilder.viewport = Vk::Structures::viewport(0.0f, 0.0f, (float) SHADOW_MAP_WIDTH, (float) SHADOW_MAP_HEIGHT, 0.0f, 1.0f);
    VkExtent2D extent{SHADOW_MAP_WIDTH,SHADOW_MAP_HEIGHT};
    pipelineBuilder.scissor = Vk::Structures::scissor(0,0,extent);
    VertexInputDescription vertexInputDescription = Vertex::get_vertex_description();
    pipelineBuilder.vertexInputInfo = Vk::Structures::pipeline_vertex_input_create_info(vertexInputDescription);
    pipelineBuilder.inputAssembly = Vk::Structures::pipeline_input_assembly_state_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
    pipelineBuilder.rasterizer = Vk::Structures::pipeline_rasterization_state_create_info(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    pipelineBuilder.multisampling = Vk::Structures::pipeline_msaa_state_create_info(VK_SAMPLE_COUNT_1_BIT); //no msaa
    pipelineBuilder.depthStencil = Vk::Structures::pipeline_depth_stencil_state_create_info(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);//VK_COMPARE_OP_LESS
    pipelineBuilder.colourBlendAttachment = Vk::Structures::pipeline_colour_blend_attachment(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, VK_FALSE);
    pipelineBuilder.pipelineLayout = shadowmapPipelineLayout;

    //add required shaders for this stage
    pipelineBuilder.shaderStages.clear();
    pipelineBuilder.shaderStages.push_back(Vk::Structures::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, shadowmap_vert_m, nullptr));
    pipelineBuilder.shaderStages.push_back(Vk::Structures::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, shadowmap_frag_test_m, nullptr));

    shadowmapPipeline = pipelineBuilder.build_pipeline(device, shadowmapRenderPass, 2);
    _swapDeletionQueue.push_function([=](){vkDestroyPipeline(device, shadowmapPipeline, nullptr);});

    vkDestroyShaderModule(device, shadowmap_frag_test_m, nullptr);
    vkDestroyShaderModule(device, shadowmap_vert_m, nullptr);
}

void Vk::Renderer::createShadowMapImage(){
    //create the layered Image with MAX_LIGHTS number of layers
    imageHelper->createImage(SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT, 1, MAX_LIGHTS, VK_SAMPLE_COUNT_1_BIT, (VkImageCreateFlagBits)0, VK_FORMAT_D32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, 
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, shadowmapImage, shadowMapImageAllocation);
    _textureDeletionQueue.push_function([=](){vmaDestroyImage(allocator, shadowmapImage, shadowMapImageAllocation);});
}

void Vk::Renderer::createShadowMapFrameBuffer(){
    VkFramebufferCreateInfo framebufferInfo;
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.pNext = NULL;
    framebufferInfo.renderPass = shadowmapRenderPass;
    framebufferInfo.attachmentCount = shadowmapImageViews.size();
    framebufferInfo.pAttachments = shadowmapImageViews.data(); //think this should be an array of VkImages
    framebufferInfo.width = SHADOW_MAP_WIDTH;
    framebufferInfo.height = SHADOW_MAP_HEIGHT;
    framebufferInfo.layers = 1;
    framebufferInfo.flags = 0;

    if(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &shadowmapFramebuffer) != VK_SUCCESS){
        throw std::runtime_error("Failed to create shadowmap framebuffer");
    }
    _swapDeletionQueue.push_function([=](){vkDestroyFramebuffer(device, shadowmapFramebuffer, nullptr);});
}

void Vk::Renderer::createShadowMapRenderPass(){
    VkAttachmentDescription attachments[2];
 
   // Depth attachment (shadow map)
   attachments[0].format = VK_FORMAT_D32_SFLOAT;
   attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
   attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
   attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
   attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
   attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
   attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   attachments[0].flags = 0;
 
   // Attachment references from subpasses
   VkAttachmentReference depth_ref;
   depth_ref.attachment = 0;
   depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
 
   // Subpass 0: shadow map rendering
   VkSubpassDescription subpass[1];
   subpass[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
   subpass[0].flags = 0;
   subpass[0].inputAttachmentCount = 0;
   subpass[0].pInputAttachments = NULL;
   subpass[0].colorAttachmentCount = 0;
   subpass[0].pColorAttachments = NULL;
   subpass[0].pResolveAttachments = NULL;
   subpass[0].pDepthStencilAttachment = &depth_ref;
   subpass[0].preserveAttachmentCount = 0;
   subpass[0].pPreserveAttachments = NULL;
 
   // Create render pass
   VkRenderPassCreateInfo renderPassInfo;
   renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
   renderPassInfo.pNext = NULL;
   renderPassInfo.attachmentCount = 1;
   renderPassInfo.pAttachments = attachments;
   renderPassInfo.subpassCount = 1;
   renderPassInfo.pSubpasses = subpass;
   renderPassInfo.dependencyCount = 0;
   renderPassInfo.pDependencies = NULL;
   renderPassInfo.flags = 0;
 
   if(vkCreateRenderPass(device, &renderPassInfo, nullptr, &shadowmapRenderPass) != VK_SUCCESS){
        throw std::runtime_error("Failed to create shadowmap render pass");
    }
    _swapDeletionQueue.push_function([=](){vkDestroyRenderPass(device, shadowmapRenderPass, nullptr);});
}*/

//void Vk::Renderer::allocateShadowMapImages(){
    /*
    uint32_t layerCounter = 0;
    //start with scene light
    //scene is layer 0 in shadowmapImage
    shadowmapImageViews.push_back(VkImageView());
    VkImageViewCreateInfo viewInfo = Vk::Structures::image_view_create_info(shadowmapImage, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, layerCounter, 1);
    if(vkCreateImageView(device, &viewInfo, nullptr, &shadowmapImageViews[layerCounter]) != VK_SUCCESS){
        throw std::runtime_error("Failed to create scene shadowMapImageView");
    }
    _textureDeletionQueue.push_function([=](){vkDestroyImageView(device, shadowmapImageViews[layerCounter], nullptr);});

    p_sceneLight->layer = layerCounter++;

    //then each spotlight
    for(int i = 0; i < p_spotLights->size(); i++){
        VkImageViewCreateInfo viewInfo = Vk::Structures::image_view_create_info(shadowmapImage, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, layerCounter, 1);
        if(vkCreateImageView(device, &viewInfo, nullptr, &shadowmapImageViews[layerCounter]) != VK_SUCCESS){
            throw std::runtime_error("Failed to create p_spotLights shadowMapImageView");
        }
        _textureDeletionQueue.push_function([=](){vkDestroyImageView(device, shadowmapImageViews[layerCounter], nullptr);});
        p_spotLights->at(i).layer = layerCounter++;
    }
    //then point lights, ommiting for now as we won't have any and i'd need to work out cube maps


    createShadowMapFrameBuffer();
    */
//}

//void Vk::Renderer::allocateDescriptorSetForShadowmap(){
    /*VkWriteDescriptorSet setWrite{};
    VkDescriptorImageInfo shadowmapImageInfo;
    shadowmapImageInfo.sampler = shadowmapSampler;
    shadowmapImageInfo.imageView = shadowmapImageViews.data(); //this should exist before this call
    shadowmapImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    setWrite = Vk::Structures::write_descriptorset(0, skyboxSet, 1, 
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &shadowmapImageInfo);
    queueSubmitMutex.lock();
    vkUpdateDescriptorSets(device,  1, &setWrite, 0, nullptr);
    queueSubmitMutex.unlock();*/
//}