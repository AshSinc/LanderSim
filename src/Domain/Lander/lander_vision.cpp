#include "lander_vision.h"

#include <opencv2/opencv.hpp>
#include "opencv2/highgui.hpp"
#include "opencv2/features2d.hpp"
#include "opencv2/xfeatures2d.hpp"
//#include "opencv2/core.hpp"
#include "opencv2/calib3d.hpp"
#include "opencv2/imgproc.hpp"

#include <thread>

#include "qtimer.h"

#include <glm/gtx/euler_angles.hpp> //for 
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "sv_randoms.h";

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
    
    /*
    For BF matcher. It takes two optional params. First one is normType. It specifies the distance measurement to be used.
    By default, it is cv.NORM_L2. It is good for SIFT, SURF etc (cv.NORM_L1 is also there). 
    For binary string based descriptors like ORB, BRIEF, BRISK etc, cv.NORM_HAMMING should be used, 
    which uses Hamming distance as measurement. If ORB is using WTA_K == 3 or 4, cv.NORM_HAMMING2 should be used.

    Second param is boolean variable, crossCheck which is false by default. If it is true, Matcher returns only those matches with value (i,j) 
    such that i-th descriptor in set A has j-th descriptor in set B as the best match and vice-versa. 
    That is, the two features in both sets should match each other. It provides consistent result, and is a good alternative to ratio test proposed by D.Lowe in SIFT paper.
    */

    //cv::Ptr<cv::BFMatcher> matcher = cv::BFMatcher::create(cv::NORM_L2, true); //use with floating point descriptors
    cv::Ptr<cv::BFMatcher> matcher = cv::BFMatcher::create(cv::NORM_HAMMING, true); //use with binary string based descriptors descriptors

    //cv::Ptr<cv::FlannBasedMatcher> matcher = cv::FlannBasedMatcher::create(); //use with binary string based descriptors descriptors
    //matcher->knnMatch(descriptorsQueue[0], descriptorsQueue[1], 2); //may not be using flann properly, and knn needs different params

    std::vector<cv::DMatch> matches;
    matcher->match(descriptorsQueue[0], descriptorsQueue[1], matches);

    //-- Draw matches
    cv::Mat matchedImage;
    drawMatches(opticsQueue[0], keypointsQueue[0], opticsQueue[1], keypointsQueue[1], matches, matchedImage,
                cv::Scalar::all(-1), cv::Scalar::all(-1), std::vector<char>(), cv::DrawMatchesFlags::DEFAULT);//, cv::DrawMatchesFlags::DEFAULT);

    //cv::imwrite("matches.jpg", matchedImage);

    //passing back to renderer
    p_mediator->renderer_assignMatToMatchingView(matchedImage); //must be a seperate mapped imageview and image

    //now we need to 
    //cv::Mat homography = cv::findHomography(Points(matched1), Points(matched2),
    //                        cv::RANSAC, ransac_thresh, inlier_mask);

    //-- Filter matches using the Lowe's ratio test
   //const float ratio_thresh = 0.75f;
    //std::vector<cv::DMatch> good_matches;
    //for (size_t i = 0; i < matches.size(); i++)
    //{
   //     if (matches[i][0].distance < ratio_thresh * matches[i][1].distance)
   //     {
   //         good_matches.push_back(matches[i][0]);
   //     }
   // }


    //-- Localize the object
    std::vector<cv::Point2f> src;
    std::vector<cv::Point2f> dst;
    for( size_t i = 0; i < matches.size(); i++ )
    {
        //-- Get the keypoints from the good matches
        src.push_back( keypointsQueue[0][ matches[i].queryIdx ].pt );
        dst.push_back( keypointsQueue[1][ matches[i].trainIdx ].pt );
    }
    cv::Mat H = findHomography(src, dst, cv::RANSAC);
    std::cout << H << " H \n";


    //construct intrinsic camera matrix
    //note this is not exactly correct, because the fov is 55.63 (used online tool to calculate these values)...
    //...for the original image 1920*1080, but we crop to 512*512
    //could maybe improve accuracy if we calculate this correctly
    cv::Mat K2 = cv::Mat::zeros(3,3, CV_64F);
    K2.at<_Float64>(0,2) = 256; //center points in pixels
    K2.at<_Float64>(1,2) = 256; //center points in pixels
    K2.at<_Float64>(0, 0) = 55.63; //horizontal fov
    K2.at<_Float64>(1, 1) = 55.63; //vertical fov
    K2.at<_Float64>(2, 2) = 1; //must be 1

    //512 x 512 at 42 degrees fov

    std::cout << "Camera matrix = " << K2 << std::endl << std::endl;

    std::vector<cv::Mat> r2;
    std::vector<cv::Mat> t2;
    std::vector<cv::Mat> n2;

    cv::decomposeHomographyMat(H, K2, r2, t2, n2);

    for (auto i: r2){
        std::cout << i << " r2 \n"; 
    }    

    glm::mat4 correctRot = Service::openCVToGlm(r2[3]);
    std::cout << glm::to_string(correctRot) << " estimated correct rot \n";

    glm::vec3 angles;
    glm::extractEulerAngleXYZ(correctRot, angles.x, angles.y, angles.z);
    std::cout << glm::to_string(angles) << " angles \n";

    //now we need to divide angles by image timer (45 sec just now)
    //and find the highest axis, set others to zero? (this would be cheating because we know there is only 1 axis of rotation, but might be all we can do)
    
    //we then also need to divide the estimated rotation matrix by image timer (45 sec just now)
    //and then multiply by 1000 (ttgo max)

    //then combine both with the same function used to calculate rotation atm

    //then pass this to gnc in place of the real rotation matrix
    
    //from opencv docs
    //https://docs.opencv.org/4.x/d9/dab/tutorial_homography.html
    /*std::vector<cv::Mat> Rs_decomp, ts_decomp, normals_decomp;
    int solutions = cv::decomposeHomographyMat(H, cameraMatrix, Rs_decomp, ts_decomp, normals_decomp);
    std::cout << "Decompose homography matrix computed from the camera displacement:" << std::endl << std::endl;
    for (int i = 0; i < solutions; i++){
      double factor_d1 = 1.0 / d_inv1;
      cv::Mat rvec_decomp;
      cv::Rodrigues(Rs_decomp[i], rvec_decomp);
      std::cout << "Solution " << i << ":" << std::endl;
      std::cout << "rvec from homography decomposition: " << rvec_decomp.t() << std::endl;
      std::cout << "rvec from camera displacement: " << rvec_1to2.t() << std::endl;
      std::cout << "tvec from homography decomposition: " << ts_decomp[i].t() << " and scaled by d: " << factor_d1 * ts_decomp[i].t() << std::endl;
      std::cout << "tvec from camera displacement: " << t_1to2.t() << std::endl;
      std::cout << "plane normal from homography decomposition: " << normals_decomp[i].t() << std::endl;
      std::cout << "plane normal at camera 1 pose: " << normal1.t() << std::endl << std::endl;
    }*/
    
    descriptorsQueue.pop_front();
    opticsQueue.pop_front();
    keypointsQueue.pop_front();
}

void Vision::detectImage(cv::Mat optics){

    opticsQueue.push_back(optics.clone()); //copy it

    //processing
    opticsQueue.back().convertTo(opticsQueue.back(), -1, 2.0, 0.0f);
   
    //detecting
    //-- Step 1: Detect the keypoints
    startT(1); //timer start
    
    //surf and sift, both use floating point descriptors so matcher should use 
    //cv::Ptr<cv::xfeatures2d::SURF> detector = cv::xfeatures2d::SURF::create(200);//min hessian
    //cv::Ptr<cv::SIFT> detector = cv::SIFT::create(200);

    //orb brief brisk use string descriptors so should use NORM HAMMING matcher
    cv::Ptr<cv::ORB> detector = cv::ORB::create(200); //num features //ORB Crashes, same error
    //cv::Ptr<cv::BRISK> detector = cv::BRISK::create(); //num features //BRISK crashes
    //what():  OpenCV(4.5.2) /home/ash/vcpkg/buildtrees/opencv4/src/4.5.2-755f235ba0.clean/modules/core/src/batch_distance.cpp:303: error: (-215:Assertion failed) K == 1 && update == 0 && mask.empty() in function 'batchDistance'

    //cv::Ptr<cv::xfeatures2d::StarDetector> detector = cv::xfeatures2d::StarDetector::create(); //star needs BriefDescriptorExtractor created

    std::vector<cv::KeyPoint> kp; //keypoints should be stored in an array or queue
    detector->detect(opticsQueue.back(), kp);
    keypointsQueue.push_back(kp);

    descriptorsQueue.emplace_back();
    if(false){ //if using StarDetector, need to specify a descriptor extractor
        cv::Ptr<cv::xfeatures2d::BriefDescriptorExtractor> extractor = cv::xfeatures2d::BriefDescriptorExtractor::create();
        extractor->compute(opticsQueue.back(), kp, descriptorsQueue.back());
    }
    else{
        detector->compute(opticsQueue.back(), kp, descriptorsQueue.back());
    }
    
    //-- Draw keypoints
    cv::Mat kpimage;
    //cv::drawKeypoints(optics, keypoints, image, cv::Scalar_<double>::all(-1), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
    cv::drawKeypoints(opticsQueue.back(), kp, kpimage);
    
    //passing back to renderer
    p_mediator->renderer_assignMatToDetectionView(kpimage);

    //if we have 2 descriptors then we can match them
    if(descriptorsQueue.size()>1)
        featureMatch();

    endT(1);
    showStats(true);

    //double t = (double)cv::getTickCount();
    // do something ...
    //t = ((double)getTickCount() - t)/getTickFrequency();
    //cout << "Times passed in seconds: " << t << endl;
    //processing = false;
}