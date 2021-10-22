#pragma once
class Mediator;
//#include "mediator.h"
struct LanderObj;
struct LandingSiteObj;


namespace Ai{
    class LanderAi{
    private:
        Mediator* p_mediator;
        LanderObj* p_lander;
        LandingSiteObj* p_landingSite;
        bool imagingActive = true;
        const float IMAGING_TIMER_SECONDS = 3.0f; //5 seconds
        float imagingTime = 0.0f;
        bool autopilot = true;
        
        void imagingTimer(float timeStep);
        void calculateMovement();
    public:
        void init(Mediator* mediator, LanderObj* lander);
        void landerSimulationTick(float timeStep);
    };
}