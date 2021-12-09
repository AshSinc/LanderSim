#include "lander_vision.h"

#include <opencv2/opencv.hpp>

#include "opencv2/highgui.hpp"
#include "opencv2/features2d.hpp"
#include "opencv2/xfeatures2d.hpp"

using namespace Lander;

void Vision::init(Mediator* mediator){
    p_mediator = mediator; 
}

void Vision::processImage(cv::Mat optics){

    //cv::Mat image;
    //processing
    optics.convertTo(optics, -1, 2.0, 0.25f);


    //detecting
    //-- Step 1: Detect the keypoints using SURF Detector
    //timer start
    int minHessian = 400;
    cv::Ptr<cv::xfeatures2d::SURF> detector = cv::xfeatures2d::SURF::create(minHessian);
    
    std::vector<cv::KeyPoint> keypoints; //keypoints shoul be stored in an array or queue
    detector->detect(optics, keypoints);
    //timer stop


    //-- Draw keypoints
    cv::Mat image;
    cv::drawKeypoints(optics, keypoints, image);
    
    //passing back to renderer
    p_mediator->renderer_assignMatToImageView(image);




    //cv::imwrite("test.jpg", image); // A JPG FILE IS BEING SAVED

    //double t = (double)cv::getTickCount();
    // do something ...
    //t = ((double)getTickCount() - t)/getTickFrequency();
    //cout << "Times passed in seconds: " << t << endl;
}