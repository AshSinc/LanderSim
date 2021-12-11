#pragma once

#include "mediator.h"
#include <deque>
#include <vector>
#include <mutex>

namespace Lander{

    class Vision{

    private:

    std::mutex processingLock;
    
    Mediator* p_mediator;
    std::deque<cv::Mat> descriptorsQueue;
    std::deque<cv::Mat> opticsQueue;
    std::deque<std::vector<cv::KeyPoint>> keypointsQueue;

    void featureMatch();
    void detectImage(cv::Mat optics);

    public:

    void init(Mediator* mediator);

    void simulationTick();

    };
}