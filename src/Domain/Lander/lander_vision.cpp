#include "lander_vision.h"

#include <opencv2/opencv.hpp>
#include "opencv2/highgui.hpp"
#include "opencv2/features2d.hpp"
#include "opencv2/xfeatures2d.hpp"

using namespace Lander;

void Vision::init(Mediator* mediator){
    p_mediator = mediator; 
    descriptorsQueue.resize(0);
    opticsQueue.resize(0);
    keypointsQueue.resize(0);
}

void Vision::featureMatch(){
    //-- Step 2: Matching descriptor vectors with a brute force matcher
    // Since SURF is a floating-point descriptor NORM_L2 is used
    cv::Ptr<cv::DescriptorMatcher> matcher = cv::DescriptorMatcher::create(cv::DescriptorMatcher::BRUTEFORCE);
    std::vector<cv::DMatch> matches;
    matcher->match(descriptorsQueue[0], descriptorsQueue[1], matches);

    //-- Draw matches
    cv::Mat matchedImage;
    drawMatches(opticsQueue[0], keypointsQueue[0], opticsQueue[1], keypointsQueue[1], matches, matchedImage);

    //passing back to renderer
    p_mediator->renderer_assignMatToMatchingView(matchedImage); //must be a seperate mapped imageview and image

    descriptorsQueue.pop_front();
    opticsQueue.pop_front();
    keypointsQueue.pop_front();
}

void Vision::processImage(cv::Mat optics){
    //processing
    optics.convertTo(optics, -1, 2.0, 0.0f);

    opticsQueue.push_back(optics);

    //detecting
    //-- Step 1: Detect the keypoints using SURF Detector
    //timer start
    std::vector<cv::KeyPoint> keypoints; //keypoints should be stored in an array or queue

    int minHessian = 400;
    //cv::Ptr<cv::xfeatures2d::SURF> detector = cv::xfeatures2d::SURF::create(minHessian);
    cv::Ptr<cv::xfeatures2d::StarDetector> detector = cv::xfeatures2d::StarDetector::create();
    //cv::Ptr<cv::SIFT> detector = cv::SIFT::create();

   
    detector->detect(optics, keypoints);
    keypointsQueue.push_back(keypoints);
    //detector->compute(optics, keypoints, descriptors);
    //detector->detectAndCompute( optics, cv::noArray(), keypoints, descriptors);

    cv::Ptr<cv::xfeatures2d::BriefDescriptorExtractor> extractor = cv::xfeatures2d::BriefDescriptorExtractor::create();

    cv::Mat descriptor;
    extractor->compute(optics, keypoints, descriptor);
    descriptorsQueue.push_back(descriptor);

    //-- Draw keypoints
    cv::Mat image;
    //cv::drawKeypoints(optics, keypoints, image, cv::Scalar_<double>::all(-1), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
    cv::drawKeypoints(optics, keypoints, image);
    
    //passing back to renderer
    p_mediator->renderer_assignMatToDetectionView(image);

    if(descriptorsQueue.size()>1)
        featureMatch();

    //cv::imwrite("test.jpg", image); // A JPG FILE IS BEING SAVED

    //double t = (double)cv::getTickCount();
    // do something ...
    //t = ((double)getTickCount() - t)/getTickFrequency();
    //cout << "Times passed in seconds: " << t << endl;
}