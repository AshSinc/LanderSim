#pragma once

class GLFWwindow;
class Mediator;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

class UiInput{
    public:      
        UiInput(Mediator& mediator);
        void mouseMoved(double xpos, double ypos);
        void scrollMoved(double xoffset, double yoffset);       
        void processKey(int key, bool repeatKeypress);
        
    private:
        Mediator& r_mediator;
        void processKeyGroupMovement(int key, bool repeatKeypress);
        bool landingSiteInput = false;
};