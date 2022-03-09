#include "lander_vision.h"
#include <opencv2/opencv.hpp>
#include "opencv2/highgui.hpp"
#include "opencv2/features2d.hpp"
#include "opencv2/xfeatures2d.hpp"
#include "opencv2/calib3d.hpp"
#include "opencv2/imgproc.hpp"
#include <thread>
#include "qtimer.h"
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "sv_randoms.h"
#include <bits/stdc++.h>

//#include "filewriter.h"

using namespace Lander;

void Vision::init(Mediator* mediator, float imageTimer, NavigationStruct* gncVars){
    p_mediator = mediator; 
    descriptorsQueue.resize(0);
    opticsQueue.resize(0);
    keypointsQueue.resize(0);
    imagingTimerSeconds = imageTimer;
    p_navStruct = gncVars;
}

void Vision::simulationTick(){
    if(active){
        if(!p_mediator->renderer_cvMatQueueEmpty()){
            std::unique_lock<std::mutex> lock(processingLock, std::try_to_lock); //try to acquire processingLock

            if(!lock.owns_lock())  //if we dont have the lock then return error
                throw std::runtime_error("Lock Failed, processing is taking longer than submission");

            cv::Mat nextImage = p_mediator->renderer_frontCvMatQueue(); //image data is not copied, just the wrapper to memory is copied
            p_mediator->renderer_popCvMatQueue(); //so we can pop the queue as well

            radiusPerImageQueue.emplace_back(p_navStruct->radiusAtOpticalCenter);
            altitudePerImageQueue.emplace_back(p_navStruct->altitude);

            std::thread thread(&Lander::Vision::detectFeatures, this, nextImage); //we run the conversions in a seperate thread, solves stuttering during processing
            thread.detach();   
        }
    }
}

void Vision::detectFeatures(cv::Mat optics){

    opticsQueue.push_back(optics.clone()); //copy it

    //preprocessing
    opticsQueue.back().convertTo(opticsQueue.back(), -1, 2.0, 0.0f);

    if(Service::OUTPUT_OPTICS){
        cv::imwrite(Service::OPTICS_PATH + "optics" + std::to_string(opticCount) + ".jpg", opticsQueue.back());
        opticCount++;
    }

    //detecting
    //-- Step 1: Detect the keypoints
    startT(1); //timer start
    
    //surf and sift, both use floating point descriptors so matcher should use NORM_L2
    cv::Ptr<cv::xfeatures2d::SURF> detector = cv::xfeatures2d::SURF::create(100);//min hessian
    //cv::Ptr<cv::SIFT> detector = cv::SIFT::create(100);

    //orb brief brisk use string descriptors so should use NORM HAMMING matcher
    //cv::Ptr<cv::ORB> detector = cv::ORB::create(200); //num features //ORB Crashes, same error
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
    
    //passing back to renderer to copy image to feature detection queue for drawing in ui_handler 
    p_mediator->renderer_assignMatToDetectionView(kpimage);

    if(Service::OUTPUT_OPTICS){
        cv::imwrite(Service::OPTICS_FEATURE_PATH + "feature" + std::to_string(featureCount) + ".jpg", kpimage);
        featureCount++;
    }

    //if we have 2 descriptors then we can match them
    if(descriptorsQueue.size()>1)
        featureMatch();

    endT(1);
    showStats(true);
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

    cv::Ptr<cv::BFMatcher> matcher = cv::BFMatcher::create(cv::NORM_L2, true); //use with floating point descriptors
    //cv::Ptr<cv::BFMatcher> matcher = cv::BFMatcher::create(cv::NORM_HAMMING, true); //use with binary string based descriptors descriptors

    //cv::Ptr<cv::FlannBasedMatcher> matcher = cv::FlannBasedMatcher::create(); //use with binary string based descriptors descriptors
    //matcher->knnMatch(descriptorsQueue[0], descriptorsQueue[1], 2); //may not be using flann properly, and knn needs different params

    std::cout << " num of descriptorsQueue[0] : " << descriptorsQueue[0].size() << "\n";
    std::cout << " num of descriptorsQueue[1] : " << descriptorsQueue[1].size() << "\n";
    
    //check the descriptors have at least some information, abandon if they do not
    if((descriptorsQueue[0].rows > 0 && descriptorsQueue[0].cols > 0) && (descriptorsQueue[1].rows > 0 && descriptorsQueue[1].cols > 0)){
        std::vector<cv::DMatch> matches;
        matcher->match(descriptorsQueue[0], descriptorsQueue[1], matches);
        //sorting
        std::sort(matches.begin(), matches.end(), compareDistance);

        //array bound check for the next operation
        int numMatchesToUse = NUM_MATCHES_TO_USE;
        if(matches.size() < NUM_MATCHES_TO_USE)
            numMatchesToUse = matches.size();
        //slice the vector and keep only the top NUM_MATCHES_TO_USE matches
        std::vector<cv::DMatch> bestMatches = std::vector<cv::DMatch>(matches.begin(), matches.begin() + numMatchesToUse);
        
        //-- Draw matches
        cv::Mat matchedImage;
        drawMatches(opticsQueue[0], keypointsQueue[0], opticsQueue[1], keypointsQueue[1], bestMatches, matchedImage,
                    cv::Scalar::all(-1), cv::Scalar::all(-1), std::vector<char>(), cv::DrawMatchesFlags::DEFAULT);//, cv::DrawMatchesFlags::DEFAULT);

        //passing back to renderer to draw the image to ui
        p_mediator->renderer_assignMatToMatchingView(matchedImage); //must be a seperate mapped imageview and image
        //cv::imwrite("matches.jpg", matchedImage);

        if(Service::OUTPUT_OPTICS){
            cv::imwrite(Service::OPTICS_MATCH_PATH + "match" + std::to_string(matchCount) + ".jpg", matchedImage);
            matchCount++;
        }

        std::vector<cv::Point2f> src;
        std::vector<cv::Point2f> dst;
        if(bestMatches.size() > MIN_NUM_FEATURES_MATCHED){
            for( size_t i = 0; i < bestMatches.size(); i++ ){
                //-- Get the keypoints from the good matches
                src.push_back( keypointsQueue[0][bestMatches[i].queryIdx ].pt);
                dst.push_back( keypointsQueue[1][bestMatches[i].trainIdx ].pt);
            }
            cv::Mat H = findHomography(src, dst, cv::RANSAC);//, 1.0, cv::noArray(), 3000);
            std::cout << H << " H \n";

            glm::vec3 bestAngularVelocityMatch = findBestAngularVelocityMatchFromDecomp(H);

            //if val is 9999 it means findBestAngularVelocityMatchFromDecomp didn't find a good match, so we will ignore it
            if(bestAngularVelocityMatch.x != 9999){
                estimatedAngularVelocities.push_back(bestAngularVelocityMatch);

                if(Service::OUTPUT_TEXT){
                    //output single estimation data to file
                    std::string time = std::to_string(p_mediator->physics_getTimeStamp());
                    std::string text = time + ":" + glm::to_string(bestAngularVelocityMatch);
                    p_mediator->writer_writeToFile("EST", text);
                }
            }

            if(estimatedAngularVelocities.size() > NUM_ESTIMATIONS_BEFORE_CALC-1)
                active = false;
        }
    }
    else
        std::cout << "Not enough keypoints found, abandoning \n";
    
    
    descriptorsQueue.pop_front();
    opticsQueue.pop_front();
    keypointsQueue.pop_front();
    radiusPerImageQueue.pop_front();
    altitudePerImageQueue.pop_front();

}

glm::vec3 Vision::findBestAngularVelocityMatchFromDecomp(cv::Mat H){
    //calculate avg altitude and radius from the 2 images
    float avgAltitude = (altitudePerImageQueue.at(0)+altitudePerImageQueue.at(1))/2;
    float avgRadius = (radiusPerImageQueue.at(0)+radiusPerImageQueue.at(1))/2;

    //construct a point used for testing, this is effectively the lander distance from surface
    glm::vec3 testPoint = glm::vec3(0, 0, avgAltitude);
    std::cout << "testPoint :" << glm::to_string(testPoint) << "\n";

    //we have no distortion or skew so calibration is not necessary, this means we are working in pixel coordinates
    //we can manually account for scale later
    cv::Mat intrinsicM = cv::Mat::eye(3,3, CV_64F);
    intrinsicM.at<_Float64>(0,2) = 256; //center point of optics in pixels
    intrinsicM.at<_Float64>(1,2) = 256; //center point of optics in pixels


    std::vector<cv::Mat> rotationM;
    std::vector<cv::Mat> translationM;
    std::vector<cv::Mat> n;
    int solutions = cv::decomposeHomographyMat(H, intrinsicM, rotationM, translationM, n);

    std::vector<glm::vec3> possibleSolutions;

    for (int i = 0; i < solutions; i++){
        std::cout << "----------------------------------------------------\n";
        std::cout << "Solution " << i << ":" << "\n";

        //get rotation matrix and convert to glm, then apply to testPoint
        glm::mat4 rotMat = Service::openCVToGlm(rotationM[i]);
        glm::vec3 rotatedPoint = rotMat*glm::vec4(testPoint,1);
        std::cout << "rotMat " << glm::to_string(rotMat) << "\n";
        std::cout << "rotatedPoint " << glm::to_string(rotatedPoint) << "\n";

        //get translation matrix and apply to testPoint
        glm::mat4 tMat = glm::mat4(1.0f);
        tMat[3][0] = translationM[i].at<_Float64>(0,0);
        tMat[3][1] = translationM[i].at<_Float64>(0,1);
        tMat[3][2] = translationM[i].at<_Float64>(0,2);
        glm::vec3 translatedPoint =  tMat*glm::vec4(testPoint,1);
        std::cout << "tMat " << glm::to_string(tMat) << "\n";
        std::cout << "translatedPoint " << glm::to_string(translatedPoint) << "\n";

        //if rotation is behind camera then discard this solution
        //if rotation is on the opposite side of asteroid then discard
        if((int)rotatedPoint.z > (int)testPoint.z){
            std::cout << "rotatedPoint is behind camera " << rotatedPoint.z << "\n";
            continue;
        }
        else if(rotatedPoint.z < 0){
            std::cout << "rotatedPoint is opposite side of asteroid " << rotatedPoint.z << "\n";
            continue;
        }

        //if translation is behind camera then discard this solution
        //if translation is on the opposite side of asteroid then discard, dont think this ever happens?
        if((int)translatedPoint.z > (int)testPoint.z){ //this maybe should have more leeway to accept small errors
            std::cout << "translatedPoint is behind camera " << translatedPoint.z << "\n";
            continue;
        }
        else if(translatedPoint.z < 0){
            std::cout << "translatedPoint is opposite side of asteroid " << translatedPoint.z << "\n";
            continue;
        }
        
        //we can extract rotation angles from rotMatrix, this will show us if there is a z rotation later
        glm::vec3 rotationAngles;
        glm::extractEulerAngleXYZ(rotMat, rotationAngles.x, rotationAngles.y, rotationAngles.z);
        std::cout << glm::to_string(rotationAngles) << " angular velocities from extractEulerAngleXYZ \n";

        //need to check against translation
        float tLength = glm::length(glm::vec2(translatedPoint));
        std::cout << "translatedPoint length : " << tLength << "\n";

        //if translation appears significant then get primary axis and calculate angular velocity
        glm::vec3 angularVelocityEstimation = glm::vec3(0);
        if(tLength > 1){

            //NOTES and working ------------

                //at scale 1, fov 5, and alt of 961, and radius of 30, 2units (world size) is about 25 pixels

                //measurements with 2x2 scale box at various distances
                //1m is 12.5 pixels at 1000m altitude
                //1m is 25 pixels at 500m altitude
                //1m is 50 pixels at 250m altitude
                //1m is 100 pixels at 125m altitude

                //we should be able to define a relationship with distance
                //inversely proportional
                //12.5 = k/1000
                //k = 12500
                //1m in pixels = 12500/distance

                //any displacement must measure linear speed at the surface, we need angular speed

                //find angular velocity
                //ω = r × v / |r|², we are assuming one axis of rotation so to simplify ω = v / r

                //example
                //altitude 950, radius 59, 84 pixels of measured movement
                //convert 84 pixels to real world units,
                //1 unit in pixels = 12500/950 = 13.157894737
                //units moved = 84/13.157894737 = 6.384
                //then find angular velocity ω = v / r
                //6.384 / 59 = 0.10820339 rad
                
                //example 2
                //altitude 450, radius 59, 180 pixels of measured movement
                //convert 180 pixels to real world units,
                //1 unit in pixels = 12500/450 = 27.777777778
                //units moved = 180/27.777777778 = 6.48
                //then find angular velocity ω = v / r
                //6.48 / 59 = 0.109830508 rad
                //good!

                //1m at fov 2.5 @ 1000m alt is 25 pixels
                //1m at fov 5 @ 1000m alt is 12.5 pixels
                //1m at fov 10 @ 1000m alt is 6 pixels
                //1m at fov 15 @ 1000m alt is 4 pixels
                //1m at fov 20 @ 1000m alt is 3 pixels

            //NOTES ------------

            float kValue = 25000/p_navStruct->asteroidScale;
            int axis = Service::getHighestAxis(glm::vec3(translatedPoint.x, translatedPoint.y, 0)); //find the significant axis
            float pixelsMoved = translatedPoint[axis]; 
            float unitsMoved = pixelsMoved/(kValue/avgAltitude); //convert pixels travelled to world units (m)
            float angularVelocity = unitsMoved/avgRadius;
            angularVelocityEstimation[axis] = angularVelocity/imagingTimerSeconds; //remember to divide by imaging timer as well to get 1s
            if(axis == 1)
                angularVelocityEstimation[1] = -angularVelocityEstimation[1]; //if y axis we need to invert it to correct for world orientation

            //need to add pixels moved, units moved, kvalue etc, probably matching the actual estimate
            if(Service::OUTPUT_TEXT){
                //output 
                //p_mediator->writer_writeToFile("PARAMS", "FOV:" + std::to_string(BASE_OPTICS_FOV*lander->asteroidScale));
            }
        }
        else if(Service::getHighestAxis(rotationAngles) == 2){ 
            //if translation is insignificant and z is highest axis in rotation, we can just use that
            angularVelocityEstimation.z = rotationAngles.z/imagingTimerSeconds;
        }

        if(glm::length(angularVelocityEstimation) != 0){
            std::cout << "angular velocities added to possible solutions\n";
            std::cout << glm::to_string(angularVelocityEstimation) << "\n";
            possibleSolutions.push_back(angularVelocityEstimation);
        }
        
        std::cout << "----------------------------------------------------\n";
    }
    
    if(possibleSolutions.size() == 0)
        possibleSolutions.push_back(glm::vec3(9999, 9999, 9999)); //dummy that is discarded
    else if(possibleSolutions.size() > 1){ 
        //if there is still more than one solution we need to determine correct one
        //can happen than + or - translations get mixed up, need to test this
        for(glm::vec3 sol : possibleSolutions)
            std::cout << glm::to_string(sol) << "\n";
    }

    return possibleSolutions[0]; //need to clean this up, last thing is to check direction of x and y rotation somehow, there is a safety in place after returning though
}

//compares distance of matches, for use in vector sorting of matches
bool Vision::compareDistance(cv::DMatch d1, cv::DMatch d2){
    return (d1.distance <= d2.distance);
}