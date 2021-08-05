#pragma once
#include <cstdint>

class Application{
    public:
        int run();
    private:
        uint32_t WIDTH = 1920; //defaults, doesnt track resizing, should have a callback function here to update these from engine
        uint32_t HEIGHT = 1080;
        bool sceneLoaded = false;
};