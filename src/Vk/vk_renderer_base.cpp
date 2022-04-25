#include "vk_renderer_base.h"
#include "vk_init_queries.h"
#include "vk_structures.h"
#include "vk_images.h"
#include "vk_pipeline.h"
#include <imgui.h> //basic gui library for drawing simple guis
#include <imgui_impl_glfw.h> //backends for glfw
#include <imgui_impl_vulkan.h> //and vulkan
#define VMA_IMPLEMENTATION 
#include <vk_mem_alloc.h> //we are using VMA (Vulkan Memory Allocator to handle allocating blocks of memory for resources)

//Base rendering set up. Heavily based on code from this ebook https://raw.githubusercontent.com/Overv/VulkanTutorial/master/ebook/Vulkan%20Tutorial%20en.pdf

Vk::RendererBase::RendererBase(GLFWwindow* windowptr, Mediator& mediator): window{windowptr}, r_mediator{mediator}{
    imageHelper = new Vk::ImageHelper(this);
}

void Vk::RendererBase::init(){
    createVkInstance();
    if (enableValidationLayers){
        Vk::Debug::Messenger::setupDebugMessenger(instance, &debugMessenger);
        _mainDeletionQueue.push_function([=]() {
		    Vk::Debug::DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
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

    initPipelines();
    createCommandBuffers();
    createSyncObjects();

    initUI();

    //here we have now loaded the basics
    //this means we should be able to end the init() here and move the rest to a loadScene() method?
}

void Vk::RendererBase::createVkInstance(){
    if (enableValidationLayers && !Vk::Init::checkValidationLayerSupport(validationLayers))
    {
        throw std::runtime_error("Validation layers requested, but not available.");
    }

    VkApplicationInfo appInfo = Vk::Structures::app_info("Vulkan Testing App Name");
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
    auto extensions = Vk::Init::getRequiredExtensions(enableValidationLayers);
    VkInstanceCreateInfo createInfo = Vk::Structures::instance_create_info(appInfo, extensions, enabledLayerNames, enabledLayerCount, next);

    //call create instance, passing pointer to createInfo and instance, instance handle is stored in VKInstance instance
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create instance!");
    }
}

//picks physical device, moved most of the queries to Vk::Init namespace in vk_init_queries.h
void Vk::RendererBase::pickPhysicalDevice(){
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
        if(Vk::Init::isDeviceSuitable(device, surface, deviceExtensions)){
            physicalDevice = device;
            msaaSamples = Vk::Init::getMaxUsableSampleCount(physicalDevice); //check our max sample count supported
            _gpuProperties = Vk::Init::getPhysicalDeviceProperties(physicalDevice);
            break;
        }
    }

    if(physicalDevice == VK_NULL_HANDLE){
        throw std::runtime_error("Failed to find suitable GPU");
    }
}

//after selecting a physical device we need to create a logical device to interface with it
//also need to specify which queues we want to create now that we've queuried available queue families
void Vk::RendererBase::createLogicalDevice(){
    //we need to have multiple VkDeviceQueueCreateInfo structs to create a queue for both families

    //find and store the queue family indices for the physical device we picked
    queueFamilyIndicesStruct = Vk::Init::findQueueFamilies(physicalDevice, surface);

    std::set<uint32_t> uniqueQueueFamilies = {queueFamilyIndicesStruct.graphicsFamily.value(), 
        queueFamilyIndicesStruct.presentFamily.value(), queueFamilyIndicesStruct.transferFamily.value()};

    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos = Vk::Structures::queue_create_infos(uniqueQueueFamilies, &queuePriority);

    //currently available drivers only support a small number of queues for each family,
    //and we dont really need mroe than one because you can create all of the command buffers omn multiple threads
    //and then submit them all at once on the main thread with a single low-overhead call
    //initialise this structure all as VK_FALSE for now, come back to this later
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE; //we want to enable the sampler anisotropy features for our sampler

    //with the previous 2 structs in place we can begin filling in the main VkDeviceCreateInfo struct
    VkDeviceCreateInfo createInfo = Vk::Structures::device_create_info(deviceFeatures, queueCreateInfos, deviceExtensions);
    
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

void Vk::RendererBase::createMemAlloc(){
    VmaAllocatorCreateInfo allocatorInfo = Vk::Structures::vma_allocator_info(physicalDevice, device, instance, allocator);  
    vmaCreateAllocator(&allocatorInfo, &allocator);
    _mainDeletionQueue.push_function([=](){vmaDestroyAllocator(allocator);});
}

//now we can use all our helper functions to create a swap chain based on their choices
void Vk::RendererBase::createSwapChain(){
    SwapChainSupportDetails swapChainSupport = Vk::Init::querySwapChainSupport(physicalDevice, surface);
    VkSurfaceFormatKHR surfaceFormat = Vk::Init::chooseSwapSurfaceFormat(swapChainSupport.formats); //HERE
    VkPresentModeKHR presentMode = Vk::Init::chooseSwapPresentMode(swapChainSupport.presentModes);

    VkExtent2D extent = Vk::Init::chooseSwapExtent(swapChainSupport.capabilities, window);
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
   
    VkSwapchainCreateInfoKHR createInfo = Vk::Structures::swapchain_create_info(surface, imageCount, surfaceFormat, extent, queueFamilyIndices, swapChainSupport.capabilities.currentTransform, presentMode);

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
void Vk::RendererBase::createSwapChainImageViews(){
    //first resize the array to the required size
    swapChainImageViews.resize(swapChainImages.size());
    //loop through all swap chain iamges
    for(size_t i = 0; i < swapChainImages.size(); i++){
        swapChainImageViews[i] = imageHelper->createImageView(swapChainImages[i], VK_IMAGE_VIEW_TYPE_2D, swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1);
        _swapDeletionQueue.push_function([=](){vkDestroyImageView(device, swapChainImageViews[i], nullptr);});
    }
}    

//ubo Descriptor layout
//we need to provide details about every descriptor binding used in the shader for pipeline creation. Just like we had to do for every vertrex attribute
//and its location index. Because we will need to use it in pipeline creation we will do this first
void Vk::RendererBase::createDescriptorSetLayouts(){

    //Create Descriptor Set layout describing the _globalSetLayout containing global scene and camera data
    //Cam is set 0 binding 0 because its first in this binding set
    //Scene is set 0 binding 1 because its second in this binding set
    VkDescriptorSetLayoutBinding camBufferBinding = Vk::Structures::descriptorset_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr);
    //binding for scene data at 1 in vertex and frag
	VkDescriptorSetLayoutBinding sceneBind = Vk::Structures::descriptorset_layout_binding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    std::vector<VkDescriptorSetLayoutBinding> bindings =  {camBufferBinding, sceneBind};
    VkDescriptorSetLayoutCreateInfo setinfo = Vk::Structures::descriptorset_layout_create_info(bindings);
    if(vkCreateDescriptorSetLayout(device, &setinfo, nullptr, &_globalSetLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create descriptor set layout");
    }
    _mainDeletionQueue.push_function([=](){vkDestroyDescriptorSetLayout(device, _globalSetLayout, nullptr);});

    //create a descriptorset layout for material
    //we will create the descriptor set layout for the objects as a seperate descriptor set
    //bind object storage buffer to 0 in vertex
    //this needs to be binding 1 because in allocatedescset for materials we add it to the write set in the second position (after texture set)
    //VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    VkDescriptorSetLayoutBinding materialsBind = Vk::Structures::descriptorset_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    bindings.clear();
    bindings = {materialsBind};
	setinfo = Vk::Structures::descriptorset_layout_create_info(bindings);
    if(vkCreateDescriptorSetLayout(device, &setinfo, nullptr, &_materialSetLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create descriptor set layout");
    }
    _mainDeletionQueue.push_function([=](){vkDestroyDescriptorSetLayout(device, _materialSetLayout, nullptr);});

    //we will create the descriptor set layout for the objects as a seperate descriptor set
    //bind object storage buffer to 0 in vertex
    VkDescriptorSetLayoutBinding objectBind = Vk::Structures::descriptorset_layout_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr);
    bindings.clear();
    bindings = {objectBind};
	setinfo = Vk::Structures::descriptorset_layout_create_info(bindings);
    if(vkCreateDescriptorSetLayout(device, &setinfo, nullptr, &_objectSetLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create descriptor set layout");
    }
    _mainDeletionQueue.push_function([=](){vkDestroyDescriptorSetLayout(device, _objectSetLayout, nullptr);});

    //we will create the descriptor set layout for the lights, could maybe be combined with materials? or object?
    //bind light storage buffer to 0 in frag
    VkDescriptorSetLayoutBinding pointLightsBind = Vk::Structures::descriptorset_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    VkDescriptorSetLayoutBinding spotLightsBind = Vk::Structures::descriptorset_layout_binding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    bindings.clear();
    bindings = {pointLightsBind, spotLightsBind};
	setinfo = Vk::Structures::descriptorset_layout_create_info(bindings);
    if(vkCreateDescriptorSetLayout(device, &setinfo, nullptr, &_lightingSetLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create descriptor set layout");
    }
    _mainDeletionQueue.push_function([=](){vkDestroyDescriptorSetLayout(device, _lightingSetLayout, nullptr);});

    //shadowmap //NOT USED ATM
    VkDescriptorSetLayoutBinding lightVPBufferBinding = Vk::Structures::descriptorset_layout_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr);
    VkDescriptorSetLayoutBinding shadowmapSamplerSetBinding = Vk::Structures::descriptorset_layout_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    //bindings =  {lightVPBufferBinding};
    bindings =  {lightVPBufferBinding, shadowmapSamplerSetBinding};
    setinfo = Vk::Structures::descriptorset_layout_create_info(bindings);
    if(vkCreateDescriptorSetLayout(device, &setinfo, nullptr, &lightVPSetLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create lightVPSetLayout descriptor set layout");
    }
    _mainDeletionQueue.push_function([=](){vkDestroyDescriptorSetLayout(device, lightVPSetLayout, nullptr);});
    //shadowmap

    VkDescriptorSetLayoutBinding diffuseSetBinding = Vk::Structures::descriptorset_layout_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    //binding for scene data at 1 in vertex and frag
	VkDescriptorSetLayoutBinding specularSetBinding = Vk::Structures::descriptorset_layout_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    bindings.clear();
    bindings =  {diffuseSetBinding, specularSetBinding};
    setinfo = Vk::Structures::descriptorset_layout_create_info(bindings);
    if(vkCreateDescriptorSetLayout(device, &setinfo, nullptr, &_multiTextureSetLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create descriptor set layout");
    }
    _mainDeletionQueue.push_function([=](){vkDestroyDescriptorSetLayout(device, _multiTextureSetLayout, nullptr);});

    VkDescriptorSetLayoutBinding skyboxSetBinding = Vk::Structures::descriptorset_layout_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    bindings.clear();
    bindings =  {skyboxSetBinding};
    setinfo = Vk::Structures::descriptorset_layout_create_info(bindings);
    if(vkCreateDescriptorSetLayout(device, &setinfo, nullptr, &_skyboxSetLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create skybox set layout");
    }
    _mainDeletionQueue.push_function([=](){vkDestroyDescriptorSetLayout(device, _skyboxSetLayout, nullptr);});
}

//Command in Vukan, like drawing operations and memory transfers, are not executed directly with function calls. 
//You have to record all the ops you want to perform in command buffer objects. The advantage is all the hard work of setting up the drawing
//commands can be done in advance and in multiple threads. After that you just have to tell Vulkan to execute the commands in the main loop
//Before we create command buffers we need a command pool. Command pools manage the memory that is used to store the buffers and 
//command buffers are alocated to them. Lets create a class member to store our VkCommandPool commandPool
void Vk::RendererBase::createCommandPools(){
    //Command buffers are execued by submitting them on one of the device queues, liek graphics or presentation queues we retrieved.
    //Each command pool can only allocate command buffers that are submitted on a single type of queue.
    VkCommandPoolCreateInfo poolInfo = Vk::Structures::command_pool_create_info(queueFamilyIndicesStruct.graphicsFamily.value());

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

    VkCommandPoolCreateInfo uploadCommandPoolInfo = Vk::Structures::command_pool_create_info(queueFamilyIndicesStruct.graphicsFamily.value());
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
void Vk::RendererBase::createColourResources(){
    VkFormat colorFormat = swapChainImageFormat;

    imageHelper->createImage(swapChainExtent.width, swapChainExtent.height, 1, 1, msaaSamples, (VkImageCreateFlagBits)0, colorFormat, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, colourImage,
        colourImageAllocation, VMA_MEMORY_USAGE_GPU_ONLY);

    colourImageView = imageHelper->createImageView(colourImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1);
    
    _swapDeletionQueue.push_function([=](){vmaDestroyImage(allocator, colourImage, colourImageAllocation);});
    _swapDeletionQueue.push_function([=](){vkDestroyImageView(device, colourImageView, nullptr);});
}

//create depth image
void Vk::RendererBase::createDepthResources(){
    // we will go with VK_FORMAT_D32_SFLOAT but will add a findSupportedFormat() first to get something supported
    VkFormat depthFormat = Vk::Init::findDepthFormat(physicalDevice);
    imageHelper->createImage(swapChainExtent.width, swapChainExtent.height, 1, 1, msaaSamples, (VkImageCreateFlagBits)0, depthFormat, VK_IMAGE_TILING_OPTIMAL, 
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageAllocation, VMA_MEMORY_USAGE_GPU_ONLY);

    depthImageView = imageHelper->createImageView(depthImage, VK_IMAGE_VIEW_TYPE_2D, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1);
    
    _swapDeletionQueue.push_function([=](){vmaDestroyImage(allocator, depthImage, depthImageAllocation);});
    _swapDeletionQueue.push_function([=](){vkDestroyImageView(device, depthImageView, nullptr);});
}

//Uniform Buffer
//We will copy new data to the unifrom buffer every frame, so we dont need a staging buffer here, it would degrade performance
//we need multiple buffers because multiple frames may be in flight and we dont want to update the buffer in preperation for the next frame
//while a previous one is still reading it! We can eithe rhave a uniform buffer per frame or per swap image. Because we need to refer to the uniform
//buffer from the command buffer that we have per swao chain image, it makes sense to also have a uniform buffer per swap chain image
void Vk::RendererBase::createUniformBuffers(){
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

        //create the spot lights Buffer one per swapchain frame
        createBuffer(sizeof(GPULightVPData) * MAX_LIGHTS, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
            _frames[i].lightVPBuffer, _frames[i].lightVPBufferAlloc);
        _swapDeletionQueue.push_function([=](){vmaDestroyBuffer(allocator, _frames[i].lightVPBuffer, _frames[i].lightVPBufferAlloc);});
    }

    const size_t sceneParamBufferSize = swapChainImages.size() * pad_uniform_buffer_size(sizeof(GPUSceneData));
    createBuffer(sceneParamBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
        _sceneParameterBuffer, _sceneParameterBufferAlloc);
    _swapDeletionQueue.push_function([=](){vmaDestroyBuffer(allocator, _sceneParameterBuffer, _sceneParameterBufferAlloc);});
}

void Vk::RendererBase::createSamplers(){
    //create a sampler for texture
	VkSamplerCreateInfo difSamplerInfo = Vk::Structures::sampler_create_info(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_TRUE, 16, 
        VK_BORDER_COLOR_INT_OPAQUE_BLACK, VK_FALSE, VK_COMPARE_OP_ALWAYS, VK_SAMPLER_MIPMAP_MODE_LINEAR, 0.0f, 0.0f, 
        static_cast<float>(_loadedTextures["lander_diff"].mipLevels)); //this shouldnt be tied to the mip level of one texture, rather it should be all texture mip levels?
	vkCreateSampler(device, &difSamplerInfo, nullptr, &diffuseSampler);
    _mainDeletionQueue.push_function([=](){vkDestroySampler(device, diffuseSampler, nullptr);});

    VkSamplerCreateInfo specSamplerInfo = Vk::Structures::sampler_create_info(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_TRUE, 16, 
        VK_BORDER_COLOR_INT_OPAQUE_BLACK, VK_FALSE, VK_COMPARE_OP_ALWAYS, VK_SAMPLER_MIPMAP_MODE_LINEAR, 0.0f, 0.0f, 
        static_cast<float>(_loadedTextures["lander_diff"].mipLevels)); //this shouldnt be tied to the mip level of one texture, rather it should be all texture mip levels?
	vkCreateSampler(device, &specSamplerInfo, nullptr, &specularSampler);
    _mainDeletionQueue.push_function([=](){vkDestroySampler(device, specularSampler, nullptr);});

    VkSamplerCreateInfo skySamplerInfo = Vk::Structures::sampler_create_info(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_TRUE, 16, 
        VK_BORDER_COLOR_INT_OPAQUE_BLACK, VK_FALSE, VK_COMPARE_OP_ALWAYS, VK_SAMPLER_MIPMAP_MODE_LINEAR, 0.0f, 0.0f, 1); //just 1 for now, 
	vkCreateSampler(device, &skySamplerInfo, nullptr, &skyboxSampler);
    _mainDeletionQueue.push_function([=](){vkDestroySampler(device, skyboxSampler, nullptr);});
}

//before we can finish creating the pipeline, we need to tell vulkan about the framebuffer attachments that will be used while rendering
//How many colour and depth buffers there will be, how many samples of each and how their contents should be handled throughout rendering ops
void Vk::RendererBase::createRenderPass(){
    //in our case we will need a colour buffer represented by one of the images from the swap chain
    VkAttachmentDescription colourAttachment = Vk::Structures::attachment_description(swapChainImageFormat, msaaSamples, VK_ATTACHMENT_LOAD_OP_CLEAR, 
        VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    //Subpasses and attachment references
    //A single rendering pass can consist of multiple subpasses. Subpasses are sunsequent rendering operations that depend on contents of
    //framebuffers in previous passes. Eg a sequence of post processing effects that are applied one after another
    //if you group these operations onto one render pass then Vulkan is able to optimize.
    //For our first triangle we will just have one subpass.
    
    //Every subpass references one or more of the attachments described in the previous sections. These references are VkAttachmentReference structs
    VkAttachmentReference colourAttachmentRef = Vk::Structures::attachment_reference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    //because we need to resolve the above multisampled image before presentation (you cant present a multisampled image because
    //it contains multiple pixel values per pixel) we need an attachment that will resolve the image back to something we can 
    //VK_IMAGE_LAYOUT_PRESENT_SRC_KHR and in standard 1 bit sampling
    //VkAttachmentDescription colourAttachmentResolve = Vk::Structs::attachment_description(swapChainImageFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_DONT_CARE, 
    //    VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VkAttachmentDescription colourAttachmentResolve = Vk::Structures::attachment_description(swapChainImageFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_DONT_CARE, 
        VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL); //CHANGED TO VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL as it is not the final layout any more, becaus ethe GUI will be drawn in another subpass later
    
    //the render pass now has to be instructed to resolve the multisampled colour image into the regular attachment
    VkAttachmentReference colourAttachmentResolveRef = Vk::Structures::attachment_reference(2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    // why is that index 2 and not 1? need to check

    // we also want to add a Subpass Dependency as described in drawFrame() to wait for VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT before proceeding 
    //specify indices of dependency and dependent subpasses, 
    //special value VK_SUBPASS_EXTERNAL refers to implicit subpass before or after the the render pass depending on whether specified in srcSubpass/dstSubpass
    //The operations that should waiti on this are in the colour attachment stage and involve the writing of the colour attachment
    //these settings will prevent the transition from happening until its actually necessary and allowed, ie when we want to start writing colours to it
    VkSubpassDependency dependency = Vk::Structures::subpass_dependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    
    //we want to include a depth attachment for depth testing
    // VK_ATTACHMENT_STORE_OP_DONT_CARE; //we dont need to store depth data as it will be discarded every pass
    // VK_IMAGE_LAYOUT_UNDEFINED; //same as colour buffer, we dont care about previous depth contents
    VkAttachmentDescription depthAttachment = Vk::Structures::attachment_description(Vk::Init::findDepthFormat(physicalDevice), msaaSamples, VK_ATTACHMENT_LOAD_OP_CLEAR, 
        VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
 
    VkAttachmentReference depthAttachmentRef = Vk::Structures::attachment_reference(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    // and here is index 1, just described in wrong order

    //we add a reference to the attachment for the first (and only) subpass
    //the index of the attachment in this array is directly referenced from the fragment shader with the layout(location = 0)out vec4 outColor directive.
    //we pass in the colourAttachmentResolveRef struct which will let the render pass define a multisample resolve operation
    //described in the struct, which will let us render the image to screen by resolving it to the right layout
    //pDepthStencilAttachment: Attachment for depth and stencil data        
    VkSubpassDescription subpass = Vk::Structures::subpass_description(VK_PIPELINE_BIND_POINT_GRAPHICS, 1, &colourAttachmentResolveRef, &colourAttachmentRef, &depthAttachmentRef);

    //now we udpate the VkRenderPassCreateInfo struct to refer to both attachments 
    //and the colourAttachmentResolve
    std::vector<VkAttachmentDescription> attachments = {colourAttachment, depthAttachment, colourAttachmentResolve};
    VkRenderPassCreateInfo renderPassInfo = Vk::Structures::renderpass_create_info(attachments, 1, &subpass, 1, &dependency);

    if(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS){
        throw std::runtime_error("Failed to create render pass");
    }
    _swapDeletionQueue.push_function([=](){vkDestroyRenderPass(device, renderPass, nullptr);});
}

//Attachments specified during render pass creation are bound by wrapping them into a VkFramebuffer object
//A framebuffer object references all the VkImageView objects that represent the attachments. We are only using one, the color attachment
//However the image we have to use for the attachment depends on which image the swapchain returns when we retrieve one for presentation
//that means we have to create a frambuffer for all the images in the swap chain and the one that corresponds to the retrieved image at draw time
// so we create a vector class member called swapChainFramebuffers, and then create the objects here
void Vk::RendererBase::createFramebuffers(){
    //start by resizing the vector to hold all the framebuffers
    swapChainFramebuffers.resize(swapChainImageViews.size());

    //then iterative through imageviews and create the framebuffers for each
    for(size_t i = 0; i < swapChainImageViews.size(); i++){
        std::vector<VkImageView> attachments = {colourImageView, depthImageView, swapChainImageViews[i]};

        VkFramebufferCreateInfo framebufferInfo = Vk::Structures::framebuffer_create_info(renderPass, attachments, swapChainExtent, 1);

        if(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS){
            throw std::runtime_error("Failed to create framebuffer");
        }
        _swapDeletionQueue.push_function([=](){vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);});
    }
}

//initialize pipelines used in rendering
//shader lighting code heavily based on examples from https://learnopengl.com/book/book_pdf.pdf
void Vk::RendererBase::initPipelines(){
    //load and create all shaders
    auto default_lit_vert_c = Vk::Init::readFile("resources/shaders/default_lit_vert.spv"); 
    auto default_lit_frag_c = Vk::Init::readFile("resources/shaders/default_lit_frag.spv");

    //auto unlit_vert_c = Vk::Init::readFile("resources/shaders/unlit_vert.spv");
    
    auto unlit_frag_c = Vk::Init::readFile("resources/shaders/unlit_frag.spv");
    auto textured_lit_frag_c = Vk::Init::readFile("resources/shaders/textured_lit_frag.spv");

    auto greyscale_lit_frag_c = Vk::Init::readFile("resources/shaders/greyscale_lit_frag.spv");//greyscale frag versions
    auto greyscale_unlit_frag_c = Vk::Init::readFile("resources/shaders/greyscale_unlit_frag.spv");//greyscale frag versions
    auto greyscale_textured_lit_frag_c = Vk::Init::readFile("resources/shaders/greyscale_textured_lit_frag.spv");//greyscale frag versions

    auto skybox_vert_c = Vk::Init::readFile("resources/shaders/skybox_vert.spv");
    auto skybox_frag_c = Vk::Init::readFile("resources/shaders/skybox_frag.spv");

    auto default_lit_vert_m = createShaderModule(default_lit_vert_c);
    auto default_lit_frag_m = createShaderModule(default_lit_frag_c);

    //auto unlit_vert_m = createShaderModule(unlit_vert_c);

    auto unlit_frag_m = createShaderModule(unlit_frag_c);
    auto textured_lit_frag_m = createShaderModule(textured_lit_frag_c);

    auto greyscale_lit_frag_m = createShaderModule(greyscale_lit_frag_c);//greyscale frag versions
    auto greyscale_unlit_frag_m = createShaderModule(greyscale_unlit_frag_c);//greyscale frag versions
    auto greyscale_textured_lit_frag_m = createShaderModule(greyscale_textured_lit_frag_c);//greyscale frag versions
    
    auto skybox_vert_m = createShaderModule(skybox_vert_c);
    auto skybox_frag_m = createShaderModule(skybox_frag_c);

    //begin default mesh pipeline creation (lit untextured mesh)
	PipelineBuilder pipelineBuilder;
    VkDescriptorSetLayout defaultSetLayouts[] = { _globalSetLayout, _objectSetLayout, _materialSetLayout, _lightingSetLayout };
    VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = Vk::Structures::pipeline_layout_create_info(4, defaultSetLayouts);
    
	//setup push constants
	VkPushConstantRange push_constant;
	//this push constant range starts at the beginning
	push_constant.offset = 0;
	//this push constant range takes up the size of a MeshPushConstants struct
	push_constant.size = sizeof(PushConstants);
	//is accessible in frag shader
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
    pipelineBuilder.viewport = Vk::Structures::viewport(0.0f, 0.0f, (float) swapChainExtent.width, (float) swapChainExtent.height, 0.0f, 1.0f);
    pipelineBuilder.scissor = Vk::Structures::scissor(0,0,swapChainExtent);
    VertexInputDescription vertexInputDescription = Vertex::get_vertex_description();
    pipelineBuilder.vertexInputInfo = Vk::Structures::pipeline_vertex_input_create_info(vertexInputDescription);
    pipelineBuilder.inputAssembly = Vk::Structures::pipeline_input_assembly_state_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
    pipelineBuilder.rasterizer = Vk::Structures::pipeline_rasterization_state_create_info(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    pipelineBuilder.multisampling = Vk::Structures::pipeline_msaa_state_create_info(msaaSamples);
    pipelineBuilder.depthStencil = Vk::Structures::pipeline_depth_stencil_state_create_info(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);//VK_COMPARE_OP_LESS
    pipelineBuilder.colourBlendAttachment = Vk::Structures::pipeline_colour_blend_attachment(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
    pipelineBuilder.pipelineLayout = meshPipelineLayout;

    //add required shaders for this stage
    pipelineBuilder.shaderStages.clear();
    pipelineBuilder.shaderStages.push_back(Vk::Structures::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, default_lit_vert_m, nullptr));
	pipelineBuilder.shaderStages.push_back(Vk::Structures::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, default_lit_frag_m, nullptr));

    VkPipeline meshPipeline = pipelineBuilder.build_pipeline(device, renderPass, 2);
    _swapDeletionQueue.push_function([=](){vkDestroyPipeline(device, meshPipeline, nullptr);});

    createMaterial(meshPipeline, meshPipelineLayout, "defaultmesh", 0);

    //that was the defaultmesh pipeline created, which has no sampler used in shader but is still lit 

    //now we will make an unlit pipeline with no texture sampler
    pipelineBuilder.shaderStages.clear();
    pipelineBuilder.shaderStages.push_back(Vk::Structures::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, default_lit_vert_m, nullptr));
	pipelineBuilder.shaderStages.push_back(Vk::Structures::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, unlit_frag_m, nullptr));

    VkPipeline unlitMeshPipeline = pipelineBuilder.build_pipeline(device, renderPass, 2);
    _swapDeletionQueue.push_function([=](){vkDestroyPipeline(device, unlitMeshPipeline, nullptr);});

    createMaterial(unlitMeshPipeline, meshPipelineLayout, "unlitmesh", 1);

    //////////////////////////////////////////
    //testing greyscale
    //now we will make an unlit pipeline with no texture sampler
    pipelineBuilder.shaderStages.clear();
    pipelineBuilder.shaderStages.push_back(Vk::Structures::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, default_lit_vert_m, nullptr));
	pipelineBuilder.shaderStages.push_back(Vk::Structures::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, greyscale_unlit_frag_m, nullptr));

    VkPipeline gsunlitMeshPipeline = pipelineBuilder.build_pipeline(device, renderPass, 2);
    _swapDeletionQueue.push_function([=](){vkDestroyPipeline(device, gsunlitMeshPipeline, nullptr);});

    createMaterial(gsunlitMeshPipeline, meshPipelineLayout, "greyscale_unlitmesh", 1);
    ////////////////////////////////////////////
    
    //create pipeline layout for the textured mesh, which has 5 descriptor sets
	//we start from  the normal mesh layout
    VkPipelineLayoutCreateInfo textured_pipeline_layout_info = mesh_pipeline_layout_info;
		
    VkDescriptorSetLayout texturedSetLayouts[] = {_globalSetLayout, _objectSetLayout, _materialSetLayout, _lightingSetLayout, _multiTextureSetLayout};

	textured_pipeline_layout_info.setLayoutCount = 5;
	textured_pipeline_layout_info.pSetLayouts = texturedSetLayouts;

	VkPipelineLayout texturedPipeLayout;

    if(vkCreatePipelineLayout(device, &textured_pipeline_layout_info, nullptr, &texturedPipeLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create texture pipeline layout");
    }
    _swapDeletionQueue.push_function([=](){vkDestroyPipelineLayout(device, texturedPipeLayout, nullptr);});

    //now for the textured pipeline
    pipelineBuilder.shaderStages.clear();
	pipelineBuilder.shaderStages.push_back(Vk::Structures::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, default_lit_vert_m, nullptr));
	pipelineBuilder.shaderStages.push_back(Vk::Structures::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, textured_lit_frag_m, nullptr));

    pipelineBuilder.pipelineLayout = texturedPipeLayout;

	VkPipeline texPipeline = pipelineBuilder.build_pipeline(device, renderPass, 2);
    _swapDeletionQueue.push_function([=](){vkDestroyPipeline(device, texPipeline, nullptr);});

    //use this texturedpipeline to create multiple textures with a name
	createMaterial(texPipeline, texturedPipeLayout, "texturedmesh1", 2);
    createMaterial(texPipeline, texturedPipeLayout, "texturedmesh2", 3);

    ///////////////////////
    //testing some greyscale material for imaging
    //greyscale textured pipeline
    pipelineBuilder.shaderStages.clear();
	pipelineBuilder.shaderStages.push_back(Vk::Structures::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, default_lit_vert_m, nullptr));
	pipelineBuilder.shaderStages.push_back(Vk::Structures::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, greyscale_textured_lit_frag_m, nullptr));

    pipelineBuilder.pipelineLayout = texturedPipeLayout;

	VkPipeline gtexPipeline = pipelineBuilder.build_pipeline(device, renderPass, 2);
    _swapDeletionQueue.push_function([=](){vkDestroyPipeline(device, gtexPipeline, nullptr);});

    //
	createMaterial(gtexPipeline, texturedPipeLayout, "greyscale_texturedmesh1", 4);
    //createMaterial(gtexPipeline, texturedPipeLayout, "texturedmesh2", 3);
    //////////////////////////////////////////////////////////////

    //Skybox
    VkDescriptorSetLayout skyboxSetLayouts[] = {_globalSetLayout, _objectSetLayout, _skyboxSetLayout};
    VkPipelineLayoutCreateInfo skybox_pipeline_layout_info = Vk::Structures::pipeline_layout_create_info(3, skyboxSetLayouts);

    if(vkCreatePipelineLayout(device, &skybox_pipeline_layout_info, nullptr, &_skyboxPipelineLayout) != VK_SUCCESS){
        throw std::runtime_error("Failed to create skybox pipeline layout");
    }
    _swapDeletionQueue.push_function([=](){vkDestroyPipelineLayout(device, _skyboxPipelineLayout, nullptr);});

     //now for the skybox pipeline
    pipelineBuilder.shaderStages.clear();
	pipelineBuilder.shaderStages.push_back(Vk::Structures::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, skybox_vert_m, nullptr));
	pipelineBuilder.shaderStages.push_back(Vk::Structures::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, skybox_frag_m, nullptr));

    pipelineBuilder.rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT; //because we are drawing on the inside of the cube

    pipelineBuilder.pipelineLayout = _skyboxPipelineLayout;

	skyboxPipeline = pipelineBuilder.build_pipeline(device, renderPass, 2);
    _swapDeletionQueue.push_function([=](){vkDestroyPipeline(device, skyboxPipeline, nullptr);});
    
    //because these wrappers just deliver the code they can be local variables and destroyed at the end of the function
    vkDestroyShaderModule(device, default_lit_vert_m, nullptr);
    vkDestroyShaderModule(device, default_lit_frag_m, nullptr);
    vkDestroyShaderModule(device, unlit_frag_m, nullptr);
    vkDestroyShaderModule(device, textured_lit_frag_m, nullptr);
    vkDestroyShaderModule(device, skybox_frag_m, nullptr);
    vkDestroyShaderModule(device, skybox_vert_m, nullptr);

    vkDestroyShaderModule(device, greyscale_lit_frag_m, nullptr);
    vkDestroyShaderModule(device, greyscale_unlit_frag_m, nullptr);
    vkDestroyShaderModule(device, greyscale_textured_lit_frag_m, nullptr);
    
}

VkShaderModule Vk::RendererBase::createShaderModule(const std::vector<char>& code){
    VkShaderModuleCreateInfo createInfo = Vk::Structures::shader_module_create_info(code);
    VkShaderModule shaderModule;
    if(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS){
        throw std::runtime_error("Failed to create shader module");
    }
    return shaderModule;
}   

Material* Vk::RendererBase::createMaterial(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name, int id){
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

Material* Vk::RendererBase::getMaterial(const std::string& name){
	//search for the object, and return nullpointer if not found
	auto it = _materials.find(name);
	if (it == _materials.end()) {
		return nullptr;
	}
	else {
		return &(*it).second;
	}
}

//Command buffer allocation
//Because one of the drawing commands involves binding the right VkFramebuffer, we will need to record a command buffer for every image in the swap chain
void Vk::RendererBase::createCommandBuffers(){
    clearValues.resize(2);
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};

    for (size_t i = 0; i < _frames.size(); i++){
		//allocate the default command buffer that we will use for rendering
		VkCommandBufferAllocateInfo cmdAllocInfo = Vk::Structures::command_buffer_allocate_info(VK_COMMAND_BUFFER_LEVEL_PRIMARY, _frames[i]._commandPool, 1);

		if(vkAllocateCommandBuffers(device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer) != VK_SUCCESS){
            throw std::runtime_error("Unable to allocate command buffer");
        }
        _swapDeletionQueue.push_function([=](){vkFreeCommandBuffers(device, _frames[i]._commandPool, 1, &_frames[i]._mainCommandBuffer);});
    }
}

//create VkSemaphores we need to use a struct, only the sType argument is required at this time
void Vk::RendererBase::createSyncObjects(){
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

void Vk::RendererBase::initUI(){
    ImGui::CreateContext(); //create ImGui context
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForVulkan(window, true);

    uint32_t imageCount = (uint32_t)swapChainImages.size();

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = instance;
    init_info.PhysicalDevice = physicalDevice;
    init_info.Device = device;
    init_info.QueueFamily = queueFamilyIndicesStruct.graphicsFamily.value();
    init_info.Queue = graphicsQueue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = descriptorPool;
    init_info.Allocator = nullptr;
    init_info.MinImageCount = imageCount-1;
    init_info.ImageCount = imageCount;
    init_info.CheckVkResultFn = nullptr; //should pass an error handling function if(result != VK_SUCCESS) throw error etc

    //ImGui_ImplVulkan_Init() needs a renderpass so we have to create one, which means we need a few structs specified first
    VkAttachmentDescription attachmentDesc = Vk::Structures::attachment_description(swapChainImageFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_LOAD, 
        VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VkAttachmentReference colourAttachmentRef = Vk::Structures::attachment_reference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colourAttachmentRef;

    //we are using a SubpassDependency that acts as synchronisation
    //so we tell Vulkan not to render this subpass until the subpass at index 0 has completed
    //or specifically at VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT stage, which is after colour output
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;  // or VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    //now we create the actual renderpass
    std::vector<VkAttachmentDescription> attachments; //we only have 1 but i set up the structs to take a vector so...
    attachments.push_back(attachmentDesc);

    VkRenderPassCreateInfo renderPassInfo = Vk::Structures::renderpass_create_info(attachments, 1, &subpass, 1, &dependency);

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &guiRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("Could not create ImGui render pass");
    }

    _swapDeletionQueue.push_function([=](){vkDestroyRenderPass(device, guiRenderPass, nullptr);});

    //then pass to ImGui implement vulkan init function
    ImGui_ImplVulkan_Init(&init_info, guiRenderPass);

    //now we copy the font texture to the gpu, we can utilize our single time command functions and transfer command pool
    VkCommandBuffer command_buffer = beginSingleTimeCommands(transferCommandPool);
    ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
    endSingleTimeCommands(command_buffer,transferCommandPool, graphicsQueue);

    VkCommandPoolCreateInfo poolInfo = Vk::Structures::command_pool_create_info(queueFamilyIndicesStruct.graphicsFamily.value());
    if(vkCreateCommandPool(device, &poolInfo, nullptr, &guiCommandPool) != VK_SUCCESS){
        throw std::runtime_error("Failed to create gui command pool");
    }   
    _swapDeletionQueue.push_function([=](){vkDestroyCommandPool(device, guiCommandPool, nullptr);
	});

    guiCommandBuffers.resize(imageCount);

	//allocate the gui command buffer
	VkCommandBufferAllocateInfo cmdAllocInfo = Vk::Structures::command_buffer_allocate_info(VK_COMMAND_BUFFER_LEVEL_PRIMARY, guiCommandPool, guiCommandBuffers.size());
    
	if(vkAllocateCommandBuffers(device, &cmdAllocInfo, guiCommandBuffers.data()) != VK_SUCCESS){
            throw std::runtime_error("Unable to allocate gui command buffers");
    }
    _swapDeletionQueue.push_function([=](){vkFreeCommandBuffers(device, guiCommandPool, guiCommandBuffers.size(), guiCommandBuffers.data());});

    //start by resizing the vector to hold all the framebuffers
    guiFramebuffers.resize(imageCount);
    //then iterative through create the framebuffers for each
    for(size_t i = 0; i < imageCount; i++){
        std::vector<VkImageView> attachment;
        attachment.push_back(swapChainImageViews.at(i));
        VkFramebufferCreateInfo framebufferInfo = Vk::Structures::framebuffer_create_info(guiRenderPass, attachment, swapChainExtent, 1);

        if(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &guiFramebuffers[i]) != VK_SUCCESS){
            throw std::runtime_error("Failed to create gui framebuffer");
        }
        _swapDeletionQueue.push_function([=](){vkDestroyFramebuffer(device, guiFramebuffers[i], nullptr);});
    }
    //r_mediator.ui_updateUIPanelDimensions(window); /not needed?
}

//helper function that begins single time commands, accepts a pool 
VkCommandBuffer Vk::RendererBase::beginSingleTimeCommands(VkCommandPool pool){
    //memory transfer operation are executed using command buffers, just like drawing commands. Therfor we must first allocate a temp command buffer.
    //You may wish to create a seperate command pool for these kinds of short lived buffers, because the implementation may be able to apply memory
    //allocation optimizations. You should use VK_COMMAND_POOL_CREATE_TRANSIENT_BIT flag during command pool generation in that case.
    VkCommandBufferAllocateInfo allocInfo = Vk::Structures::command_buffer_allocate_info(VK_COMMAND_BUFFER_LEVEL_PRIMARY, pool, 1);

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    //now we immediately start recording the command buffer
    //good practice to signal this is a one time submission.
    VkCommandBufferBeginInfo beginInfo = Vk::Structures::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

//helper function that ends single time commands, accepts a commandBuffer and a queue to submit to and wait on
void Vk::RendererBase::endSingleTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool pool, VkQueue queue) {
    vkEndCommandBuffer(commandBuffer);

    //now we execute the command buffer to complete the transfer
    VkSubmitInfo submitInfo = Vk::Structures::submit_info(&commandBuffer);

    queueSubmitMutex.lock();
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE); //error here, we needed to submit to the transfer queue
    
    vkQueueWaitIdle(queue);
    queueSubmitMutex.unlock();

    //unlike draw command there are no events we need to wait on this time. We just want to execute on the buffers immediately.
    //we could use a fence and wait with vkWaitForFences, or simply wait for the transfer queue to become idle with vkQueueWaitIdle.
    //A fence would allow us to schedule multiple transfers simultaniously and wait for all of them to complete instead of executing one at a time.
    //Which may allow the driver to optimize.

    //dont forget to clean up the buffer used for the transfer operation
    //these are single time use commands so they will be on a transientcommand pool
    vkFreeCommandBuffers(device, pool, 1, &commandBuffer);
}

//Buffer Creation
//Buffers in Vulkan are regions of memory used for storing arbitrary data that can be read by the graphics card. 
//They can be used to stor vertex data which we will do here. But can be used for other thigns which we will look at later.
//Unlike other Vulkan objects that we have used so far, buffers do not automatically allocate memory for themselves
//We have a lot of control but have to manage the memory
//because we are going to create multiple buffers we will seperate the buffer creation into a helper function
//parms, size of buffer in bytes, uage flags, propert flags, buffer and buffer memory pointers to store the buffer and memory addresses
void Vk::RendererBase::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage vmaUsage, VkBuffer& buffer, VmaAllocation& allocation){
    uint32_t queueFamilyIndices[] = {queueFamilyIndicesStruct.graphicsFamily.value(), queueFamilyIndicesStruct.transferFamily.value()};
    //both graphics and transfer queue families involved so count 2 and concurrent sharing for now
    VkBufferCreateInfo bufferInfo = Vk::Structures::buffer_create_info(size, usage, VK_SHARING_MODE_CONCURRENT, 2, queueFamilyIndices);
    
    VmaAllocationCreateInfo vmaAllocInfo = Vk::Structures::vma_allocation_create_info(vmaUsage);

    vmaCreateBuffer(allocator, &bufferInfo, &vmaAllocInfo, &buffer, &allocation, nullptr);
}
    
void Vk::RendererBase::createVertexBuffer(){       //uploading vertex data to the gpu    
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

    //queueSubmitMutex.lock();

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
    _modelBufferDeletionQueue.push_function([=](){vmaDestroyBuffer(allocator, vertexBuffer, vertexBufferAllocation);});

    //now copy the contents of staging buffer into vertexBuffer
    copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

    //now the data is copied from staging to gpu memory we should cleanup
    vmaDestroyBuffer(allocator, stagingBuffer, stagingBufferAllocation);

    //queueSubmitMutex.unlock();
}

//Index Buffer
//Used to store array of pointers to the vertices we want to draw
//Just like vertex data we need a VkBuffer for the GPU to be able to access them
//This function is almost identical to createVertexBuffer
//only differences are specifying indices and the size and data. and VK_BUFFER_USAGE_INDEX_BUFFER_BIT as the usage type
void Vk::RendererBase::createIndexBuffer(){
    VkDeviceSize bufferSize = sizeof(allIndices[0]) * allIndices.size();
    VkBuffer stagingBuffer;  //create our staging buffer      
    VmaAllocation stagingBufferAllocation;

    //queueSubmitMutex.lock();

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, stagingBufferAllocation);
    void* mappedData;
    vmaMapMemory(allocator, stagingBufferAllocation, &mappedData);
    memcpy(mappedData, allIndices.data(), (size_t) bufferSize);
    //memcpy(mappedData, indices.data(), (size_t) bufferSize);
    vmaUnmapMemory(allocator, stagingBufferAllocation);
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT |  VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
    VMA_MEMORY_USAGE_GPU_ONLY, indexBuffer, indexBufferAllocation);
    _modelBufferDeletionQueue.push_function([=](){vmaDestroyBuffer(allocator, indexBuffer, indexBufferAllocation);});
    copyBuffer(stagingBuffer, indexBuffer, bufferSize);
    //now the data is copied from staging to gpu memory we should cleanup
    vmaDestroyBuffer(allocator, stagingBuffer, stagingBufferAllocation);

    //queueSubmitMutex.unlock();
}

void Vk::RendererBase::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(transientCommandPool);
    VkBufferCopy copyRegion = Vk::Structures::buffer_copy(size);
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
void Vk::RendererBase::createDescriptorPool(){
    //create a descriptor pool that will hold 10 uniform buffers
	std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 20},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 20}
    };
    VkDescriptorPoolCreateInfo poolInfo = Vk::Structures::descriptor_pool_create_info(poolSizes, 40);
    //now we can create the descriptor pool and store in the member variable descriptorPool
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool");
    }
    _swapDeletionQueue.push_function([=](){vkDestroyDescriptorPool(device, descriptorPool, nullptr);});
}


//Descriptor Set
//described with a VkDescriptorSetAllocateInfo struct. You need to specify the descriptor pool to allocate from, the number of
//descriptor sets to allocate, and the descriptor layout to base them on
void Vk::RendererBase::createDescriptorSets(){//do we really need one per swapchain image??????????????????
    for(size_t i = 0; i < swapChainImages.size(); i++){
        //descriptors that refer to buffers, like our uniform buffer descriptor, are configured with VkDescriptorBufferInfo
        //it specifies the buffer and region within it that contains the data for the descriptor
        
        //allocate globalDescriptor set for each frame
		VkDescriptorSetAllocateInfo globalSetAllocInfo = Vk::Structures::descriptorset_allocate_info(descriptorPool, &_globalSetLayout);
		if(vkAllocateDescriptorSets(device, &globalSetAllocInfo, &_frames[i].globalDescriptor) != VK_SUCCESS){
            throw std::runtime_error("Failed to allocate memory for globalDescriptor set");
        }

        //allocate a storage object descriptor set for each frame
        VkDescriptorSetAllocateInfo objectSetAllocInfo = Vk::Structures::descriptorset_allocate_info(descriptorPool, &_objectSetLayout);
		if(vkAllocateDescriptorSets(device, &objectSetAllocInfo, &_frames[i].objectDescriptor) != VK_SUCCESS){
            throw std::runtime_error("Failed to allocate memory for object descriptor set");
        }
        //allocate a uniform material descriptor set for each frame
        VkDescriptorSetAllocateInfo materialSetAllocInfo = Vk::Structures::descriptorset_allocate_info(descriptorPool, &_materialSetLayout);
		if(vkAllocateDescriptorSets(device, &materialSetAllocInfo, &_frames[i].materialSet) != VK_SUCCESS){
            throw std::runtime_error("Failed to allocate memory for material decriptor set");
        }
        //allocate a uniform lighting descriptor set for each frame
        VkDescriptorSetAllocateInfo lightingSetAllocInfo = Vk::Structures::descriptorset_allocate_info(descriptorPool, &_lightingSetLayout);
		if(vkAllocateDescriptorSets(device, &lightingSetAllocInfo, &_frames[i].lightSet) != VK_SUCCESS){
            throw std::runtime_error("Failed to allocate memory for lighting descriptor set");
        }
        //allocate a uniform light VP matrix descriptor set for each frame
        VkDescriptorSetAllocateInfo lightVPSetAllocInfo = Vk::Structures::descriptorset_allocate_info(descriptorPool, &lightVPSetLayout);
		if(vkAllocateDescriptorSets(device, &lightVPSetAllocInfo, &_frames[i].lightVPSet) != VK_SUCCESS){
            throw std::runtime_error("Failed to allocate memory for light VP descriptor set");
        }

        std::array<VkWriteDescriptorSet, 7> setWrite{};

        //information about the buffer we want to point at in the descriptor
		VkDescriptorBufferInfo cameraInfo;
		cameraInfo.buffer = _frames[i].cameraBuffer; //it will be the camera buffer
		cameraInfo.offset = 0; //at 0 offset
		cameraInfo.range = sizeof(GPUCameraData); //of the size of a camera data struct
        //binding camera uniform buffer to 0
        setWrite[0] = Vk::Structures::write_descriptorset(0, _frames[i].globalDescriptor, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &cameraInfo);

        VkDescriptorBufferInfo sceneInfo;
		sceneInfo.buffer = _sceneParameterBuffer;
		sceneInfo.offset = 0; //we are using dynamic uniform buffer now so we dont hardcode the offset, 
        //instead we tell it the offset when binding the descriptorset at drawFrame (when we need to rebind it) hence dynamic
        //and they will be reading from the same buffer
		sceneInfo.range = sizeof(GPUSceneData);
        //binding scene uniform buffer to 1, we are using dynamic offsets so set the flag
        setWrite[1] = Vk::Structures::write_descriptorset(1, _frames[i].globalDescriptor, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, &sceneInfo);

        //object buffer info describes the location and the size
        VkDescriptorBufferInfo objectBufferInfo;
		objectBufferInfo.buffer = _frames[i].objectBuffer;
		objectBufferInfo.offset = 0;
		objectBufferInfo.range = sizeof(GPUObjectData) * MAX_OBJECTS;
        //notice we bind to 0 as this is part of a seperate descriptor set
        setWrite[2] = Vk::Structures::write_descriptorset(0, _frames[i].objectDescriptor, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &objectBufferInfo);

        VkDescriptorBufferInfo materialBufferInfo;
		materialBufferInfo.buffer = _frames[i].materialBuffer;
		materialBufferInfo.offset = 0;
		materialBufferInfo.range = sizeof(GPUMaterialData) * MATERIALS_COUNT;
        setWrite[3] = Vk::Structures::write_descriptorset(0, _frames[i].materialSet, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &materialBufferInfo);

        VkDescriptorBufferInfo pointlightsBufferInfo;
		pointlightsBufferInfo.buffer = _frames[i].pointLightParameterBuffer;
		pointlightsBufferInfo.offset = 0;
		pointlightsBufferInfo.range = sizeof(GPUPointLightData) * MAX_LIGHTS;
        setWrite[4] = Vk::Structures::write_descriptorset(0, _frames[i].lightSet, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &pointlightsBufferInfo);

        VkDescriptorBufferInfo spotLightsBufferInfo;
		spotLightsBufferInfo.buffer = _frames[i].spotLightParameterBuffer;
		spotLightsBufferInfo.offset = 0;
		spotLightsBufferInfo.range = sizeof(GPUSpotLightData) * MAX_LIGHTS;
        setWrite[5] = Vk::Structures::write_descriptorset(1, _frames[i].lightSet, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &spotLightsBufferInfo);

        //light VP buffer only holds the light view projection matrix for shadowmap creation
        VkDescriptorBufferInfo lightVPBufferInfo;
		lightVPBufferInfo.buffer = _frames[i].lightVPBuffer;
		lightVPBufferInfo.offset = 0;
		lightVPBufferInfo.range = sizeof(GPULightVPData) * MAX_LIGHTS;
        //notice we bind to 0 as this is part of a seperate descriptor set
        setWrite[6] = Vk::Structures::write_descriptorset(0, _frames[i].lightVPSet, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &lightVPBufferInfo);

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(setWrite.size()), setWrite.data(), 0, nullptr);    
    }

    //allocate a skybox descriptor set for each frame
    VkDescriptorSetAllocateInfo skyboxSetAllocInfo = Vk::Structures::descriptorset_allocate_info(descriptorPool, &_skyboxSetLayout);
    if(vkAllocateDescriptorSets(device, &skyboxSetAllocInfo, &skyboxSet) != VK_SUCCESS){
        throw std::runtime_error("Failed to allocate memory for skybox descriptor set");
    }
}

void Vk::RendererBase::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free){
    if (commandBuffer == VK_NULL_HANDLE){
        return;
    }

    if(vkEndCommandBuffer(commandBuffer)  != VK_SUCCESS){
        throw std::runtime_error("Failed to end command buffer in Vk::RendererBase::flushCommandBuffer");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    // Create fence to ensure that the command buffer has finished executing
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = (VkFenceCreateFlags)0;

    VkFence fence;
    if(vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS){
        throw std::runtime_error("Failed to create fence in Vk::RendererBase::flushCommandBuffer");
    }
    // Submit to the queue
    if(vkQueueSubmit(queue, 1, &submitInfo, fence) != VK_SUCCESS){
        throw std::runtime_error("Failed to submit queue in Vk::RendererBase::flushCommandBuffer");
    }
    // Wait for the fence to signal that command buffer has finished executing
    //10000000 is 10ms
    if(vkWaitForFences(device, 1, &fence, VK_TRUE, 10000000) != VK_SUCCESS){
        throw std::runtime_error("Failed to wait for fence in Vk::RendererBase::flushCommandBuffer");
    }

    vkDestroyFence(device, fence, nullptr);

    if (free){
        vkFreeCommandBuffers(device, pool, 1, &commandBuffer);
    }
}

Vk::RenderStats& Vk::RendererBase::getRenderStats(){
    return renderStats;
}

size_t Vk::RendererBase::pad_uniform_buffer_size(size_t originalSize){
	// Calculate required alignment based on minimum device offset alignment
	size_t minUboAlignment = _gpuProperties.limits.minUniformBufferOffsetAlignment;
	size_t alignedSize = originalSize;
	if (minUboAlignment > 0) {
		alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
	}
	return alignedSize;
}

void Vk::RendererBase::flushSceneBuffers(){
    vkDeviceWaitIdle(device); //make sure device is idle and not mid draw
    _modelBufferDeletionQueue.flush(); //clear vertices/indices data 
}

void Vk::RendererBase::flushTextures(){
    vkDeviceWaitIdle(device); //make sure device is idle and not mid draw
    _textureDeletionQueue.flush();
}

//because we need to free swapChain resources before recreating it, we should have a seperate function to andle the cleanup that can be called from recreateSwapChain()
void Vk::RendererBase::flushSwapChain(){
    _swapDeletionQueue.flush();
}

void Vk::RendererBase::cleanup(){
    vkDeviceWaitIdle(device); //make sure device is idle and not mid draw

    flushSceneBuffers();
    flushTextures();

    flushSwapChain();

    p_uiHandler->cleanup(); //would be better if this wasnt here

    _mainDeletionQueue.flush();

    delete imageHelper;

    vkDestroySurfaceKHR(instance, surface, nullptr); //surface should be destroyed before instance
    vkDestroyInstance(instance, nullptr); //all other vulkan resources should be cleaned u//helper function that gets max usable sample count for MSAA
    glfwDestroyWindow(window); //then destroy window
    glfwTerminate(); //then glfw
}

//Swap Chain Recreation
//if the window surface changes (eg it is resized) then the swap chain becomes incompatible with it
//we have to catch these events and then the swap chain must be recreated. Here we recreate the chain
void Vk::RendererBase::recreateSwapChain(){
    int width = 0, height = 0;
    while(width == 0 || height == 0){ //handles minimizing, just pause if minimized
        glfwGetFramebufferSize(window, &width, &height); //keep checking the size
        glfwWaitEvents(); //between waiting for any glfw window events
    }
    vkDeviceWaitIdle(device); //first we wait until the resources are not in use
    flushSwapChain(); //clean old swapchain first
    createSwapChain(); //then create the swap chain
    createSwapChainImageViews(); //image views then need to be recreated because they are based directly on the swap chain images
    createColourResources();
    createDepthResources();
    createUniformBuffers(); //recreate our uniform buffers
    createDescriptorPool(); //recreate desc pool
    createDescriptorSets(); //recreate the desc sets 
    createRenderPass();  //render pass needs to be recreated because it depends on the format of the swap chain images
    createFramebuffers(); //framebuffers depend directly on swap chain images
    initPipelines(); //viewport and scissor rectangle size is specified during graphics pipeline creation, so the pipeline needs rebuilt    
    createCommandBuffers(); //cammand buffers depend directly on swap chain images

    initUI(); //needed?

    //probs then need to link descriptors etc back to the materials?

    //ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount); //do i need to resize imgui? probably
}