#pragma once
#include <cstdint>
#include "world_physics.h"
#include "ui_input.h"
#include "world_camera.h"
#include "ui_handler.h"
#include "mediator.h"
#include <memory>

namespace Vk{
    class WindowHandler;
}

class GLFWwindow;

class Application{
    public:
        int run();
        void loadScene();
        void endScene();
        void resetScene();
    private:
        Vk::WindowHandler windowHandler;
        GLFWwindow* window; //pointer to the window, freed on cleanup() in VulkanRenderer just now

        //construct components
        Mediator mediator = Mediator();
        WorldPhysics worldPhysics = WorldPhysics(mediator);
        WorldCamera worldCamera = WorldCamera(mediator); //should make LockableCamera(objects&) class extend a CameraBase class,
        UiInput uiInput = UiInput(mediator);
        uint32_t WIDTH = 1920; //defaults, doesnt track resizing, should have a callback function here to update these from engine
        uint32_t HEIGHT = 1080;
        bool sceneLoaded = false;

        std::unique_ptr<IScene> scene;

        void bindWindowCallbacks();
        void unbindWindowCallbacks();
};