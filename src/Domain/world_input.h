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
        void setCamera(WorldCamera* camera);
        void toggleMenu();
        void changeSimSpeed(int pos, bool pause);
        void mouseMoved(double xpos, double ypos);
        void scrollMoved(double xoffset, double yoffset);        
    private:
        WorldState* p_world;
        //GLFWwindow* p_window;
        UiHandler* p_uiHandler;        
        WorldCamera* p_camera;
        int objectFocusIndex = 2; //start at the first worldState.object[i] that we want to be able to track, have to set in update function too
        float lastMouseX, lastMouseY, yaw, pitch;
        bool firstMouseInput = true;
        bool freelook = false;
        int winHeight;
        int winWidth;       
        const float MOUSELOOK_SENSITIVITY = 0.1f;
        const float MOUSESCROLL_SENSITIVITY = 0.5f;
};