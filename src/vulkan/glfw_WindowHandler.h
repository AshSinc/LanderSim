#ifndef GLFW_WINDOW_HANDLER_D
#define GLFW_WINDOW_HANDLER_D
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class WindowHandler{
    public:
    GLFWwindow* window;
    GLFWwindow* initWindow(const uint32_t width,const uint32_t height, const char* windowName);
    void createSurface(VkInstance instance, GLFWwindow* window, VkSurfaceKHR* surface);
};
static void frameBufferResizeCallback(GLFWwindow* window, int width, int height);

#endif