#pragma once
class Mediator;
struct LanderObj;
struct LandingSiteObj;

namespace Ai{
    class LanderAi{
    private:
        const float IMAGING_TIMER_SECONDS = 2.0f; //5 seconds
        const float LANDER_SPEED_CAP = 3.0f; //2m/s may also work but 3.0f seems good

        bool autopilot = true;
        bool imagingActive = true;
        float imagingTime = 0.0f;

        Mediator* p_mediator;
        LanderObj* p_lander;
        LandingSiteObj* p_landingSite;
        
        void imagingTimer(float timeStep);
        void calculateMovement();

    public:
        void init(Mediator* mediator, LanderObj* lander);
        void landerSimulationTick(float timeStep);
    };
}