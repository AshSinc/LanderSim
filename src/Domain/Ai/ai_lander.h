#pragma once
class Mediator;
struct LanderObj;
struct LandingSiteObj;
class btRigidBody;
namespace Ai{
    class LanderAi{
    private:
        const float IMAGING_TIMER_SECONDS = 2.0f; //5 seconds
        const int DESCENT_TICKER = 30; //number of IMAGING_TIMER_SECONDS cycles waited until begin final descent
        const float LANDER_SPEED_CAP = 10.0f; //2m/s may also work but 3.0f seems good
        const float APPROACH_DISTANCE = 50.0f;
        const float FINAL_APPROACH_DISTANCE_REDUCTION = 0.25f;

        float scaledApproachDistance = APPROACH_DISTANCE;

        bool lockRotation = true;
        bool autopilot = true;
        bool imagingActive = true;
        float imagingTime = 0.0f;
        int descentTicker = 0;

        bool shouldDescend = false;
        bool touchdownBoost = false;

        Mediator* p_mediator;
        LanderObj* p_lander;
        LandingSiteObj* p_landingSite;
        
        void imagingTimer( float timeStep);
        void calculateMovement();
        void calculateRotation();
        void setRotation();


    public:
        void init(Mediator* mediator, LanderObj* lander);
        void landerSimulationTick(btRigidBody* body, float timeStep);
        void setAutopilot(bool b);
    };
}