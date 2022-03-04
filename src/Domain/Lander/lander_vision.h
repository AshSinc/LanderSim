#pragma once

#include "mediator.h"
#include <deque>
#include <vector>
#include <mutex>
#include "lander_navstruct.h"

namespace Lander{

    class Vision{

    private:

    NavigationStruct* p_navStruct;

    float imagingTimerSeconds = 0.0f;

    int NUM_ESTIMATIONS_BEFORE_CALC = 20;

    int NUM_MATCHES_TO_USE = 50;

    std::deque<float> radiusPerImageQueue;
    std::deque<float> altitudePerImageQueue;

    std::mutex processingLock;
    
    Mediator* p_mediator;
    std::deque<cv::Mat> descriptorsQueue;
    std::deque<cv::Mat> opticsQueue;
    std::deque<std::vector<cv::KeyPoint>> keypointsQueue;

    void featureMatch();
    void detectImage(cv::Mat optics);

    std::vector<glm::vec3> estimatedAngularVelocities;

    static bool compareDistance(cv::DMatch d1, cv::DMatch d2);

    glm::vec3 findBestAngularVelocityMatchFromDecomp(cv::Mat H);

    void cameraPoseFromHomography(const cv::Mat& H, cv::Mat& pose);

    public:

    std::vector<glm::vec3> getEstimatedAngularVelocities(){return estimatedAngularVelocities;};

    bool active = true;

    void init(Mediator* mediator, float imageTimer, NavigationStruct* gncVars);

    void simulationTick();
    

    };
}