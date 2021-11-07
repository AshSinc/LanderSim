#pragma once
class Mediator;
struct LanderObj;
struct LandingSiteObj;
class btRigidBody;
namespace Ai{
    class LanderAi{
    private:
        const float IMAGING_TIMER_SECONDS = 1.0f;
        const int DESCENT_TICKER = 30; //number of IMAGING_TIMER_SECONDS cycles waited until begin final descent
        const float LANDER_SPEED_CAP = 20.0f;

        ///
        const float INITIAL_APPROACH_DISTANCE = 50.0f;
        const float APPROACH_DISTANCE_REDUCTION = 5.0f;

        ///
        const float FINAL_APPROACH_DISTANCE = 15.0f;
        const float FINAL_APPROACH_DISTANCE_REDUCTION = 0.25f;

        //float scaledFinalApproachDistance = 10.0f;

        bool lockRotation = true;
        bool autopilot = true;
        bool imagingActive = true;
        float imagingTime = 0.0f;
        int descentTicker = 0;
        float approachDistance = 0.0f;
        float minRange = 0.0f;
        float maxRange = 0.0f;

        bool shouldDescend = false;
        //bool touchdownBoost = false;

        bool USE_DIST_DELTA_CORRECTION = true;
        float previousDistance = -1.0f;
        float averageDistToGround = -1.0f;
        float deltaToGround [10];
        int distancesToGround_index = 0;

        Mediator* p_mediator;
        LanderObj* p_lander;
        LandingSiteObj* p_landingSite;
        
        void imagingTimer(float timeStep);
        void calculateMovement(float timeStep);
        void calculateRotation();
        void setRotation();
        bool checkApproachAligned(float distanceToLandingSite);
        float getAverageDistanceReduction();
        float getDistanceROC();

    public:
        void init(Mediator* mediator, LanderObj* lander);
        void landerSimulationTick(btRigidBody* body, float timeStep);
        void setAutopilot(bool b);
        void setImaging(bool b);
    };
}