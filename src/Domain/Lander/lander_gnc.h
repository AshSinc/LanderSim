#pragma once

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
class Mediator;

namespace Lander{

    struct NavigationStruct{
        float approachDistance;
        glm::vec3 angularVelocity;
        glm::vec3 gravityVector;
        glm::mat4 landerTransformMatrix;
        glm::vec3 velocityVector;
        glm::vec3 landingSitePos;
        glm::vec3 landingSiteUp;
    };

    class GNC{
    private:

        NavigationStruct* p_navStruct;

        //should be consts but, c++
        //float INITIAL_APPROACH_DISTANCE = 50.0f;
        float APPROACH_ANGLE = 0.15f; // radians
        float TF_TIME = 900.0f; //1000 seconds of flight time is roughly accurate for current starting distance

        float tf = TF_TIME;
        float t = 0.0f;
        float tgo = TF_TIME;

        bool shouldDescend = false;

        Mediator* p_mediator;

        glm::vec3 ZEM_ZEV_Control(float timeStep);
        glm::vec3 preApproach();

        glm::vec3 getZEM(glm::vec3 rf, glm::vec3 r, glm::vec3 v);
        glm::vec3 getZEV(glm::vec3 vf, glm::vec3 v);
        glm::vec3 getZEMZEVAccel(glm::vec3 zem, glm::vec3 zev);

        void calculateVectorsAtTime(float time);
        glm::vec3 projectedLandingSitePos;
        glm::vec3 projectedVelocityAtTf;
        glm::vec3 projectedLandingSiteUp;

        bool checkApproachAligned(glm::vec3 futureLsUp, glm::vec3 futureLsPos);

        void updateTgo(float timeStep);
        glm::mat4 constructRotationMatrixAtTf(float ttgo);
        glm::vec3 predictFinalLandingSitePos(glm::mat4 rotationM);
        glm::vec3 predictFinalLandingSiteUp(glm::mat4 rotationM);
        glm::vec3 stabiliseCurrentPos();
        glm::vec3 slewToRotation(glm::vec3 up, float time);
        //glm::quat rotateTowards(glm::quat q1, glm::quat q2, float maxAngle);

        glm::mat4 rotationMatrixAtTf;
        
    public:
        GNC(){};
        void init(Mediator* mediator, NavigationStruct* gncVars);
        glm::vec3 getThrustVector(float timeStep);
        glm::vec3 getProjectedUpVector();
        glm::mat4 getProjectedLSRotationMatrix(){return rotationMatrixAtTf;};
    };
}