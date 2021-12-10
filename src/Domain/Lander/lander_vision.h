#pragma once

#include <deque>
#include <vector>

#include "mediator.h"

namespace Lander{

    class Vision{

    private:
    
    //std::deque<?whatdoes OpenCV need and what does ImGUI need> imageQueue;
    //think IMGUI only needs a VKImage along with descriptor set and a sampler
    //OpenCV needs either file or copied from a buffer, 
    //we can copy from vkImage to a vkBuffer, need to try and load it with OpenCV

    Mediator* p_mediator;
    std::deque<cv::Mat> descriptorsQueue;
    std::deque<cv::Mat> opticsQueue;
    std::deque<std::vector<cv::KeyPoint>> keypointsQueue;

    void featureMatch();

    public:

    //Vision::Vision(Mediator* mediator);

    void init(Mediator* mediator);

    void processImage(cv::Mat image);

    };
}