#pragma once
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include "lander_gnc.h"
#include "lander_vision.h"
#include <deque>
#include <mutex>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include "lander_navstruct.h"

class Mediator;
struct LanderObj;
struct LandingSiteObj;
class btRigidBody;

namespace Lander{
    //holds lander impulse request --should be in obj_lander but i'd need to modify flow a lot
    class CPU{
    private:
        bool useRotationEstimation = true;
        bool estimateComplete = false;

        std::mutex landerBoostQueueLock;
        std::deque<LanderBoostCommand> landerBoostQueue;

        GNC gnc = GNC();
        NavigationStruct navStruct = NavigationStruct();

        Vision cv = Vision();

        const float BASE_OPTICS_FOV = 2.5f;

        const float INITIAL_APPROACH_DISTANCE = 50.0f;
        
        const float BOOST_STRENGTH = 1.0f;
        const float LANDER_BOOST_CAP = 5.0f; //physical limit for an individual boost, regardless of requested boost 

        const float GNC_TIMER_SECONDS = 1.0f; //still not great if higher than 1s, not sure why yet
        const float IMAGING_TIMER_SECONDS = 45.0f;  

        bool lockRotation = true;
        bool reactionWheelEnabled = false;
        
        bool imagingActive = true;
        bool gncActive = true;
        float imagingTime = 0.0f;
        int imgCount = 0; //only used for resetting test plane every first image
        float gncTime = 0.0f;
        float approachDistance = 0.0f;
        glm::vec3 asteroidAngularVelocity;

        Mediator* p_mediator;
        LanderObj* p_lander;
        LandingSiteObj* p_landingSite;
        
        //contol timers
        bool imagingTimer(float timeStep);
        bool gncTimer(float timeStep);

        void setRotation();

        //boost methods
        void applyImpulse(btRigidBody* rigidbody, LanderBoostCommand boost);
        void applyTorque(btRigidBody* rigidbody, LanderBoostCommand boost);
        LanderBoostCommand& popLanderImpulseQueue();
        bool landerImpulseRequested();
        void slewToLandingSiteOrientation();
        glm::quat rotateTowards(glm::quat q1, glm::quat q2, float maxAngle);

        void showEstimationStats();
        glm::vec3 getFinalEstimatedAngularVelocity();
        
    public:
        void toggleRotationEstimation(){useRotationEstimation = !useRotationEstimation;};
        void init(Mediator* mediator, LanderObj* lander);
        void simulationTick(btRigidBody* body, float timeStep);
        void setAutopilot(bool b);
        void setImaging(bool b);
        void addImpulseToLanderQueue(float duration, float x, float y, float z, bool torque);
    };
}