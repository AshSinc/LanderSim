#pragma once
#include <GLFW/glfw3.h>
#include <vector>
//#include "world_state.h"
//#include "ui_handler.h"

//forward declare
class WorldCamera;
class WorldObject;
class UiHandler;
class WorldState;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

class WorldInput{
    public:      
        WorldInput(GLFWwindow* win);
        void updateFixedLookPosition();
        void changeFocus();
        void setWorld(WorldState* world);
        void setUiHandler(UiHandler* uiHandler);
        void toggleMenu(GLFWwindow* window);
        void changeSimSpeed(int pos, bool pause);
        void mouseMoved(double xpos, double ypos);
        void scrollMoved(double xoffset, double yoffset);        
    private:
        WorldState* p_world;
        GLFWwindow* p_window;
        UiHandler* p_uiHandler;
        int selectedSimSpeedIndex = 2;
        float SIM_SPEEDS[7] {0.25f,0.5f,1,2,4,8,16};
        
        WorldCamera* p_camera;
        int objectFocusIndex = 2; //start at the first worldState.object[i] that we want to be able to track, have to set in update function too
        float lastMouseX, lastMouseY, yaw, pitch;
        bool firstMouseInput = true;
        bool freelook = false;
        int winHeight;
        int winWidth;
        std::vector<WorldObject>* p_objects;
        const float CAMERA_SPEED = 5;
        const float MOUSELOOK_SENSITIVITY = 0.1f;
        const float MOUSESCROLL_SENSITIVITY = 0.5f;
        float fixedLookRadius = 2;
        float fixedObjectScaleFactor = 1;

        
        
};