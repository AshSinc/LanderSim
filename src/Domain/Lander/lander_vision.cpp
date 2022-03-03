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
#include <glm/gtx/vector_angle.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "sv_randoms.h"

#include <bits/stdc++.h>

#include "obj_lander.h" //not ideal to include this here, only used to get lander up, to work out camera plane

using namespace Lander;

void Vision::init(Mediator* mediator, float imageTimer){
    p_mediator = mediator; 
    descriptorsQueue.resize(0);
    opticsQueue.resize(0);
    keypointsQueue.resize(0);
    imagingTimerSeconds = imageTimer;
}

void Vision::simulationTick(){
    if(active){
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
}

//compares distance of matches, for use in vector sorting of matches
bool Vision::compareDistance(cv::DMatch d1, cv::DMatch d2){
    return (d1.distance < d2.distance);
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

    //now we should sort matches based on distance, then take the top 30 or so
    //for (size_t i = 0; i < matches.size(); i++){
    //    std::cout << matches[i].distance << " , at pos " << i << " match distance \n";
    //}

    //sorting
    std::sort(matches.begin(), matches.end(), compareDistance);
    
    //for (size_t i = 0; i < matches.size(); i++){
    //    std::cout << matches[i].distance << " , at pos " << i << " sorted match distance \n";
    //}

    //array bound check for the next operation
    int numMatchesToUse = NUM_MATCHES_TO_USE;
    if(matches.size() < NUM_MATCHES_TO_USE)
        numMatchesToUse = matches.size();
    //slice the vector and keep only the top NUM_MATCHES_TO_USE matches (30 or so)
    std::vector<cv::DMatch> bestMatches = std::vector<cv::DMatch>(matches.begin(), matches.begin() + numMatchesToUse);
    //std::vector<cv::DMatch> bestMatches = matches;
    

    //-- Draw matches
    cv::Mat matchedImage;
    drawMatches(opticsQueue[0], keypointsQueue[0], opticsQueue[1], keypointsQueue[1], bestMatches, matchedImage,
                cv::Scalar::all(-1), cv::Scalar::all(-1), std::vector<char>(), cv::DrawMatchesFlags::DEFAULT);//, cv::DrawMatchesFlags::DEFAULT);

    //passing back to renderer to draw the image to ui
    p_mediator->renderer_assignMatToMatchingView(matchedImage); //must be a seperate mapped imageview and image
    //cv::imwrite("matches.jpg", matchedImage);

    std::vector<cv::Point2f> src;
    std::vector<cv::Point2f> dst;
    for( size_t i = 0; i < bestMatches.size(); i++ ){
        //-- Get the keypoints from the good matches
        src.push_back( keypointsQueue[0][ bestMatches[i].queryIdx ].pt );
        dst.push_back( keypointsQueue[1][ bestMatches[i].trainIdx ].pt );
    }
    cv::Mat H = findHomography(src, dst, cv::RANSAC);//, 1.0, cv::noArray(), 3000);
    //cv::Mat H = findHomography(src, dst, 0);
    std::cout << H << " H \n";

    glm::vec3 bestAngularVelocityMatch = findBestAngularVelocityMatchFromDecomp(H);

    //cv::Mat camPose;
    //cameraPoseFromHomography(H, camPose);

    //if val is 9999 it means findBestAngularVelocityMatchFromDecomp didn't find a good match, so we will ignore it
    if(bestAngularVelocityMatch.x != 9999){
        estimatedAngularVelocities.push_back(bestAngularVelocityMatch);
    }

    

    if(estimatedAngularVelocities.size() > NUM_ESTIMATIONS_BEFORE_CALC-1)
        active = false;
    
    descriptorsQueue.pop_front();
    opticsQueue.pop_front();
    keypointsQueue.pop_front();
}

glm::vec3 Vision::findBestAngularVelocityMatchFromDecomp(cv::Mat H){

    //construct intrinsic camera matrix
    //image is 1920x1080 originally, with 45 degree vertical fov
    //this is then cropped to 512x512

    //f_x = x*0.5 / tan(a_x / 2) //https://codeyarns.com/tech/2015-09-08-how-to-compute-intrinsic-camera-matrix-for-a-camera.html
    //where a_x is fov
    //because we crop that must reduce fov by 1080/512 = 2.109375
    //so new fov is 45/2.109375 = 21.33333
    //f_x = 512*0.5 / tan(21.33333 / 2) = 1359.175722654

    cv::Mat K2 = cv::Mat::zeros(3,3, CV_64F);
    K2.at<_Float64>(0,2) = 256; //center points in pixels
    K2.at<_Float64>(1,2) = 256; //center points in pixels
    //K2.at<_Float64>(0, 0) = 55.63; //horizontal fov
    //K2.at<_Float64>(1, 1) = 55.63; //vertical fov
    //K2.at<_Float64>(0, 0) = 666.9; //horizontal fov
    //K2.at<_Float64>(1, 1) = 666.9; //vertical fov
    K2.at<_Float64>(0, 0) = 1359.175722654; //horizontal fov
    K2.at<_Float64>(1, 1) = 1359.175722654; //vertical fov
    K2.at<_Float64>(2, 2) = 1; //must be 1

    //std::cout << "Camera matrix = " << K2 << std::endl << std::endl;
    //cv::Mat pose;
    //cameraPoseFromHomography(H, pose);
    //std::cout << pose << " pose \n";

    std::vector<cv::Mat> r2;
    std::vector<cv::Mat> t2;
    std::vector<cv::Mat> n2;

    int solutions = cv::decomposeHomographyMat(H, K2, r2, t2, n2);

    glm::vec3 angles;
    //glm::vec4 testPoint = glm::vec4(0,0,1000,0);
    glm::vec3 testPoint = glm::vec3(0, 0, 1000);

    //from opencv docs
    //https://docs.opencv.org/4.x/d9/dab/tutorial_homography.html

    //NOTE FOR REPORT :
    //This approach of using homography decomposition is maybe not viable without the ability to clamp the decomposeHomography function to ignore estimating translation?
    //even then it still is not ideal, im sure it must be possible mathematically though
    //combined with pixel values changing as the asteroid rotates making feature matching more difficult.
    //feature matching is sometimes very accurate and other times completely confused. cause not known

    //std::cout << "Homography matrix = " << H << std::endl << std::endl;

    std::vector<glm::vec3> possibleSolutions;

    for (int i = 0; i < solutions; i++){
        cv::Mat rvec_decomp;
        cv::Rodrigues(r2[i], rvec_decomp);
        
        
        std::cout << "----------------------------------------------------" << std::endl;
        std::cout << "Solution " << i << ":" << std::endl;

        

        glm::mat3 rotMat = Service::openCVToGlm(r2[i]);

        std::cout << glm::to_string(rotMat) << " rotmat \n";

        //glm::vec3 rotatedPoint = rotMat*glm::vec3(0,0,1000);
        glm::vec3 rotatedPoint = rotMat*testPoint;
        std::cout << "rotatedPoint " << glm::to_string(rotatedPoint) << std::endl;
        int rotLength = (int) glm::length(rotatedPoint);
        std::cout << "rotatedPoint length is " << rotLength << std::endl;

        glm::extractEulerAngleXYZ(Service::openCVToGlm(r2[i]), angles.x, angles.y, angles.z);
        std::cout << glm::to_string(angles) << " angular velocities from extractEulerAngleXYZ \n";

        
        
        if(Service::getHighestAxis(angles) == 2){
            //then we have a Z rotation which can be taken

            //still need to check for correct solution
            if (rotLength == 1000){
                //this is a valid rotation because the test distance remains the same from origin
                //glm::extractEulerAngleXYZ(Service::openCVToGlm(r2[i]), angles.x, angles.y, angles.z);
                std::cout << "angular velocities added to possible solutions\n";
                possibleSolutions.push_back(angles);
            }
        }
        //else{
            
            //we have an x or y rotation and need to dig deeper

            float scale = 1000.0f;

            std::cout << t2[i] << " t mat \n";


            glm::mat4 tmat = glm::mat4(1.0f);
            tmat[3][0] = t2[i].at<_Float64>(0,0)*scale;
            tmat[3][1] = t2[i].at<_Float64>(0,1)*scale;
            tmat[3][2] = t2[i].at<_Float64>(0,2)*scale;

            
            
            std::cout << "tmat " << glm::to_string(tmat) << std::endl;
            std::cout << "tvec from homography decomposition: " << t2[i].t() << "\n";

            glm::vec3 translatedPoint =  tmat*glm::vec4(testPoint,1);
            std::cout << "translatedPoint " << glm::to_string(translatedPoint) << std::endl;
            int tLength = (int) glm::length(translatedPoint);
            std::cout << "translatedPoint length is " << tLength << std::endl;

            //glm::vec3 rotatedtranslatedPoint =  tmat*glm::vec4(translatedPoint,1);
            //std::cout << "rotatedtranslatedPoint " << glm::to_string(rotatedtranslatedPoint) << std::endl;

            //glm::vec3 rtPoint = (testPoint*rotMat) * tmat;
            //std::cout << "rotate then translate Point " << tLength << std::endl;

            //+z coordinate maps to +x rotational velocity
            //-z coordinate maps to -x rotational velocity

            //+x coordinate maps to +z rotational velocity

            glm::vec3 normTranslatedPoint = glm::normalize(translatedPoint);
            glm::vec3 normTestPoint = glm::normalize(testPoint);

            //float angleX = glm::angle(normTranslatedPoint.x, normTestPoint.x);
            //float angleY = glm::angle(normTranslatedPoint.y, normTestPoint.y);
            //float angleZ = glm::angle(normTranslatedPoint.z, normTestPoint.z);

            //std::cout << " angle between x is : " << angleX << "\n";
            //std::cout << " angle between y is : " << angleY << "\n";
            //std::cout << " angle between z is : " << angleZ << "\n";
            
            float angle = glm::orientedAngle(normTranslatedPoint,normTestPoint,glm::vec3(0, 0, 1));
            std::cout << " angle between vectors is : " << angle << "\n";

        //}

        //float scale = 1000;

        //std::cout << "rvec from homography decomposition: " << rvec_decomp.t() << std::endl;
        //std::cout << "tvec : " << t2[i] <<  std::endl ;//"
        //std::cout << "tvec from homography decomposition: " << t2[i].t() <<  std::endl ;//"
        //std::cout << "and scaled by d: " << scale * t2[i].t() << std::endl;

        //cv::Mat tMatrix = cv::Mat::eye(4,4, CV_64F);
        //tMatrix.at<_Float64>(3,0) = t2[i].at<_Float64>(0,0);
        //tMatrix.at<_Float64>(3,1) = t2[i].at<_Float64>(0,1);
        //tMatrix.at<_Float64>(3,2) = t2[i].at<_Float64>(0,2);
        //glm::mat4 tmat = glm::mat4(1.0f);
        //tmat[3][0] = t2[i].at<_Float64>(0,0);
        //tmat[3][1] = t2[i].at<_Float64>(0,1);
        //tmat[3][2] = t2[i].at<_Float64>(0,2);

        //glm::mat4 tmat = Service::openCVToGlm(tMatrix);
        //std::cout << "tmatrix " << tMatrix << std::endl;
        //std::cout << "tmat " << glm::to_string(tmat) << std::endl;

        //glm::vec3 translatedPoint =  tmat*testPoint;
        //std::cout << "translatedPoint " << glm::to_string(translatedPoint) << std::endl;
        //int tLength = (int) glm::length(translatedPoint);
        //std::cout << "translatedPoint length is " << tLength << std::endl;

        //std::cout << "plane normal from homography decomposition: " << n2[i].t() << std::endl;
        //std::cout << "plane normal at camera 1 pose: " << normal1.t() << endl << endl;


        //so if rvez Z is the primary component then we just use that,
        //if not then we use tvec instead and calculate the change in angle


        //if (rotLength == 1000){
            //this is a valid rotation because the test distance remains the same from origin
            //glm::extractEulerAngleXYZ(Service::openCVToGlm(r2[i]), angles.x, angles.y, angles.z);
        //    std::cout << "angular velocities added to possible solutions\n";
        //    possibleSolutions.push_back(angles);
        //}

        //glm::mat4 tMat = Service::openCVToGlm(t2[i]);
        //glm::vec4 translatedPoint = tMat*testPoint;
        //std::cout << "translatedPoint " << glm::to_string(translatedPoint) << std::endl;
        //std::cout << "translatedPoint length is " << glm::length(translatedPoint) << std::endl;
        
        //std::cout << "----------------------------------------------------" << std::endl;
        //std::cout << "rvec from homography decomposition: " << rvec_decomp.t() << std::endl;
        //std::cout << "tvec from homography decomposition: " << t2[i].t() <<  std::endl ;//" and scaled by d: " << factor_d1 * t2[i].t() << std::endl;
        //std::cout << "plane normal from homography decomposition: " << n2[i].t() << std::endl;
        //std::cout << "plane normal of camera: " << glm::to_string(-p_mediator->scene_getLanderObject()->up) << std::endl;
        //std::cout << "plane normal at camera 1 pose: " << normal1.t() << std::endl << std::endl;
    }
    
    //we need to swap x and y values velocities and negate them, this is because we are above the asteroid and the camera doesn't know that
    //ISSUE - if we change lander position this should be changed
    if(possibleSolutions.size() > 0){
        //just taking first of possible solutions for now.
        float temp = possibleSolutions[0].x;
        possibleSolutions[0].x = -possibleSolutions[0].y/imagingTimerSeconds;
        possibleSolutions[0].y = -temp/imagingTimerSeconds;
        possibleSolutions[0].z = possibleSolutions[0].z/imagingTimerSeconds;
        std::cout << glm::to_string(possibleSolutions[0]) << " best guess angular velocities\n";
    }
    else{
        possibleSolutions.push_back(glm::vec3(9999, 9999, 9999));
    }

    return possibleSolutions[0];
}

void Vision::detectImage(cv::Mat optics){

    opticsQueue.push_back(optics.clone()); //copy it

    //processing
    opticsQueue.back().convertTo(opticsQueue.back(), -1, 2.0, 0.0f);
   
    //detecting
    //-- Step 1: Detect the keypoints
    startT(1); //timer start
    
    //surf and sift, both use floating point descriptors so matcher should use NORM_L2
    //cv::Ptr<cv::xfeatures2d::SURF> detector = cv::xfeatures2d::SURF::create(10);//min hessian
    //cv::Ptr<cv::SIFT> detector = cv::SIFT::create(100);

    //orb brief brisk use string descriptors so should use NORM HAMMING matcher
    //cv::Ptr<cv::ORB> detector = cv::ORB::create(200); //num features //ORB Crashes, same error
    cv::Ptr<cv::BRISK> detector = cv::BRISK::create(); //num features //BRISK crashes
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

    //p_mediator->renderer_assignMatToDetectionView(opticsQueue.back()); //for testing without kps drawn

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