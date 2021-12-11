#include "lander_vision.h"

#include <opencv2/opencv.hpp>
#include "opencv2/highgui.hpp"
#include "opencv2/features2d.hpp"
#include "opencv2/xfeatures2d.hpp"

#include <thread>

using namespace Lander;

void Vision::init(Mediator* mediator){
    p_mediator = mediator; 
    descriptorsQueue.resize(0);
    opticsQueue.resize(0);
    keypointsQueue.resize(0);
}

void Vision::simulationTick(){
    if(!p_mediator->renderer_cvMatQueueEmpty()){
        std::unique_lock<std::mutex> lock(processingLock, std::try_to_lock); //try to acquire processingLock

        if(!lock.owns_lock()){  //if we dont have the lock then return error
            throw std::runtime_error("Lock Failed, processing is taking longer than submission");
        }

        cv::Mat nextImage = p_mediator->renderer_frontCvMatQueue(); //image data is not copied, just the wrapper to memory is copied
        p_mediator->renderer_popCvMatQueue(); //so we can pop the queue as well

        std::thread thread(&Lander::Vision::detectImage, this, nextImage); //we run the conversions in a seperate thread, reduces stuttering
        thread.detach();   
    }
}

void Vision::featureMatch(){
    //-- Step 2: Matching descriptor vectors with a brute force matcher
    // Since SURF is a floating-point descriptor NORM_L2 is used
        
    //cv::Ptr<cv::BFMatcher> matcher = cv::BFMatcher::create(cv::NORM_L2, true); //use with floating point descriptors
    cv::Ptr<cv::BFMatcher> matcher = cv::BFMatcher::create(cv::NORM_HAMMING, true); //use with binary string based descriptors descriptors

    /*
    For BF matcher, first we have to create the BFMatcher object using cv.BFMatcher(). It takes two optional params. First one is normType. 
    It specifies the distance measurement to be used. By default, it is cv.NORM_L2. It is good for SIFT, SURF etc (cv.NORM_L1 is also there). 
    For binary string based descriptors like ORB, BRIEF, BRISK etc, cv.NORM_HAMMING should be used, which used Hamming distance as measurement. 
    If ORB is using WTA_K == 3 or 4, cv.NORM_HAMMING2 should be used.
    */

    std::vector<cv::DMatch> matches;
    matcher->match(descriptorsQueue[0], descriptorsQueue[1], matches);

    //-- Draw matches
    cv::Mat matchedImage;
    drawMatches(opticsQueue[0], keypointsQueue[0], opticsQueue[1], keypointsQueue[1], matches, matchedImage);

    //cv::imwrite("matches.jpg", matchedImage);

    //passing back to renderer
    p_mediator->renderer_assignMatToMatchingView(matchedImage); //must be a seperate mapped imageview and image
    
    descriptorsQueue.pop_front();
    opticsQueue.pop_front();
    keypointsQueue.pop_front();
}

void Vision::detectImage(cv::Mat optics){

    //cv::Mat image = ;
    opticsQueue.push_back(optics.clone());

    //processing
    opticsQueue.back().convertTo(opticsQueue.back(), -1, 2.0, 0.0f);
   

    //detecting
    //-- Step 1: Detect the keypoints using SURF Detector
    //timer start
    std::vector<cv::KeyPoint> keypoints; //keypoints should be stored in an array or queue

    //surf and sift, both use floating point descriptors so matcher should use 
    //cv::Ptr<cv::xfeatures2d::SURF> detector = cv::xfeatures2d::SURF::create(400);//min hessian
    //cv::Ptr<cv::SIFT> detector = cv::SIFT::create();

    //cv::Ptr<cv::xfeatures2d::StarDetector> detector = cv::xfeatures2d::StarDetector::create();
    
    

    cv::Ptr<cv::ORB> detector = cv::ORB::create(200); //num features
   
    //detector->detect(opticsQueue.back(), keypoints);
    //keypointsQueue.push_back(keypoints);
    detector->detect(opticsQueue.back(), keypoints);
    keypointsQueue.push_back(keypoints);
    //detector->compute(optics, keypoints, descriptors);
    //detector->detectAndCompute( optics, cv::noArray(), keypoints, descriptors);

    cv::Mat descriptor;
    descriptorsQueue.emplace_back();
    if(false){
        cv::Ptr<cv::xfeatures2d::BriefDescriptorExtractor> extractor = cv::xfeatures2d::BriefDescriptorExtractor::create();
        //extractor->compute(opticsQueue.back(), keypoints, descriptor);
        extractor->compute(opticsQueue.back(), keypoints, descriptorsQueue.back());
    }
    else{
        //detector->compute(opticsQueue.back(), keypoints, descriptor);
        detector->compute(opticsQueue.back(), keypoints, descriptorsQueue.back());
    }
    
    //-- Draw keypoints
    cv::Mat kpimage;
    //cv::drawKeypoints(optics, keypoints, image, cv::Scalar_<double>::all(-1), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
    cv::drawKeypoints(opticsQueue.back(), keypoints, kpimage);
    
    //passing back to renderer
    p_mediator->renderer_assignMatToDetectionView(kpimage);

    std::cout << descriptorsQueue.size() << " desc size\n";

    if(descriptorsQueue.size()>1)
        featureMatch();

    //double t = (double)cv::getTickCount();
    // do something ...
    //t = ((double)getTickCount() - t)/getTickFrequency();
    //cout << "Times passed in seconds: " << t << endl;
    //processing = false;
}