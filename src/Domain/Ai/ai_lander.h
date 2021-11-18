#pragma once
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
class Mediator;
struct LanderObj;
struct LandingSiteObj;
class btRigidBody;

namespace Ai{
    class LanderAi{
    private:
        const float IMAGING_TIMER_SECONDS = 1.0f; //still not great if higher than 1s, not sure why yet
        const int DESCENT_TICKER = 30; //number of IMAGING_TIMER_SECONDS cycles waited until begin final descent
        const float LANDER_SPEED_CAP = 20.0f;

        const float INITIAL_APPROACH_DISTANCE = 50.0f;
        const float APPROACH_DISTANCE_REDUCTION = 5.0f;

        const float FINAL_APPROACH_DISTANCE = 15.0f;
        const float FINAL_APPROACH_DISTANCE_REDUCTION = 0.25f;

        const float APPROACH_ANGLE = 0.15f; // radians


        bool lockRotation = true;
        bool autopilot = true;
        bool imagingActive = true;
        float imagingTime = 0.0f;
        int descentTicker = 0;
        float approachDistance = 0.0f;
        float minRange = 0.0f;
        float maxRange = 0.0f;

        float tf = 1000.0f;
        float t = 0.0f;
        float tgo = 1000.0f;

        bool shouldDescend = false;

        glm::vec3 asteroidAngularVelocity;

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
        void linearControl(float timeStep);
        void ZEM_ZEV_Control(float timeStep);
        void preApproach();

        glm::vec3 getZEM(glm::vec3 rf, glm::vec3 r, glm::vec3 v);
        glm::vec3 getZEV(glm::vec3 vf, glm::vec3 v);
        glm::vec3 getZEMZEVAccel(glm::vec3 zem, glm::vec3 zev);

        void calculateVectorsAtTime(float time);
        glm::vec3 projectedLandingSitePos;
        glm::vec3 projectedVelocityAtTf;
        glm::vec3 projectedLandingSiteUp;

        void calculateRotation();
        void setRotation();
        bool checkApproachAligned(float distanceToLandingSite);
        bool checkApproachAligned(glm::vec3 futureLsUp, glm::vec3 futureLsPos);
        float getAverageDistanceReduction();
        float getDistanceROC();
        void updateTgo();
        glm::mat4 constructRotationMatrixAtTf(float ttgo);
        glm::vec3 predictFinalLandingSitePos(glm::mat4 rotationM);
        glm::vec3 predictFinalLandingSiteUp(glm::mat4 rotationM);
        void stabiliseCurrentPos();
    public:
        void init(Mediator* mediator, LanderObj* lander);
        void landerSimulationTick(btRigidBody* body, float timeStep);
        void setAutopilot(bool b);
        void setImaging(bool b);
    };
}