#include "lander_vision.h"

#include <opencv2/opencv.hpp>

#include "opencv2/highgui.hpp"
#include "opencv2/features2d.hpp"
#include "opencv2/xfeatures2d.hpp"

using namespace Lander;

void Vision::init(Mediator* mediator){
    p_mediator = mediator; 
    processImage();
}

void Vision::processImage(){
    cv::Mat image;
    image = cv::imread( "filename.ppm", 1 );
    if ( !image.data )
    {
        printf("No image data \n");
        return;
    }

    cv::Vec3b intensity = image.at<cv::Vec3b>(0, 0);
    std::cout << intensity << "\n";

    //-- Step 1: Detect the keypoints using SURF Detector
    int minHessian = 400;
    cv::Ptr<cv::xfeatures2d::SURF> detector = cv::xfeatures2d::SURF::create( minHessian );
    std::vector<cv::KeyPoint> keypoints;

    detector->detect( image, keypoints );
    //-- Draw keypoints
    cv::Mat img_keypoints;
    cv::drawKeypoints( image, keypoints, img_keypoints );

    cv::imwrite("test.jpg", img_keypoints); // A JPG FILE IS BEING SAVED


    //cv::namedWindow("Display Image", cv::WINDOW_AUTOSIZE );
    //cv::imshow("Display Image", image);
    //cv::waitKey(0);

    //double t = (double)getTickCount();
    // do something ...
    //t = ((double)getTickCount() - t)/getTickFrequency();
    //cout << "Times passed in seconds: " << t << endl;
}