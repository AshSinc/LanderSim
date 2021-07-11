#pragma once
#include <GLFW/glfw3.h>
#include "world_state.h"


class WorldInput{
    public:
        void processKeyboardInput();
        static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void mouse_callback(GLFWwindow* window, double xpos, double ypos);
        static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
        static void updateFixedLookPosition();
        static void changeFocus();
        WorldInput(uint32_t& winHeight, uint32_t& winWidth);
    private:
        static WorldState& world;
        static WorldCamera& camera;
        static int objectFocusIndex;
        static float lastMouseX, lastMouseY, yaw, pitch;
        static bool firstMouseInput;
        static bool freelook;
        uint32_t& winHeight;
        uint32_t& winWidth;
        static std::vector<WorldObject>& objects;
        static const float CAMERA_SPEED;
        static const float MOUSELOOK_SENSITIVITY;
        static const float MOUSESCROLL_SENSITIVITY;
        static float fixedLookRadius;
        static float fixedObjectScaleFactor;
};