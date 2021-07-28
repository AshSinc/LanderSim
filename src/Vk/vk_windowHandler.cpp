#include "vk_windowHandler.h"
#include <stdexcept>

GLFWwindow* Vk::WindowHandler::initWindow(const uint32_t width,const uint32_t height, const char* windowName){
    glfwInit();                                   //always first call
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); //tell it not to create OpenGL context
    window = glfwCreateWindow(width, height, windowName, nullptr, nullptr); //create window, 4th is specific monitor, 5th is for OpenGL
    //exlicit resize callback
    //because glfw doesnt know how to call a member function with the right _this_ pointer to our HelloTriangleApplication instance
    //we get a GLFWwindow* reference in the callback and we can set another arbitrary pointer to our _this_ instance with glfwSetWindowUserPointer()
    
    //glfwSetWindowUserPointer(window, this); 
    //glfwSetFramebufferSizeCallback(window, frameBufferResizeCallback); //add a callback to explicitly check for resizeing the window
    
    //DISABLED explicit resize testing as i was getting validation errors reported when resizing and the surface was out of sync with the other components
    return window;
}

//VkSurfaceKHR surface; //represents an abstract type of surface to present rendered images to
void Vk::WindowHandler::createSurface(VkInstance instance, GLFWwindow* window, VkSurfaceKHR* surface){
    //glfwCreateWindowSurface simply takes params and sets up a platform specific surface for us
    //params are the VkInstance, the GLFW window pointer, custom allocators and poitner to VkSurfaceKHR variable to store the handle.
    //it simply passes throw the VkResult from the relevant platform call 
    if(glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS){
        throw std::runtime_error("Failed to create window surface");
    }
}

//callback from glfw on window resize
static void Vk::frameBufferResizeCallback(GLFWwindow* window, int width, int height){
    // we can retrieve our stored instance pointer we submitted to glfw earlier with glfwGetWindowUserPointer()
    //disabled for now to avoid importing vk_engine again, causing vma implementation to double up
    //auto app = reinterpret_cast<VulkanEngine*>(glfwGetWindowUserPointer(window));
    //app->frameBufferResized = true; //now we set frameBufferResized to true, which will checked in our drawFrame()
}