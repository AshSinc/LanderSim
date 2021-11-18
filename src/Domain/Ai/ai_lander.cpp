#include "ai_lander.h"
#include "mediator.h"
#include "obj_lander.h"
#include "obj_landingSite.h"
#define GLM_FORCE_RADIANS //makes sure GLM uses radians to avoid confusion
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES //forces GLM to use a version of vec2 and mat4 that have the correct alignment requirements for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE //forces GLM to use depth range of 0 to 1, instead of -1 to 1 as in OpenGL
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/norm.hpp> //for glm::length2
#include <glm/gtx/vector_angle.hpp> //for vectorangle
#include <chrono>
#include <thread>
#include <iostream>

using namespace Ai;

void LanderAi::init(Mediator* mediator, LanderObj* lander){
    p_mediator = mediator;
    p_lander = lander;
    p_landingSite = p_mediator->scene_getLandingSiteObject();
    approachDistance = INITIAL_APPROACH_DISTANCE*(p_mediator->scene_getFocusableObject("Asteroid").scale.x*2);
    minRange = approachDistance - 5;
    maxRange = approachDistance + 5;
    previousDistance = FINAL_APPROACH_DISTANCE;
    asteroidAngularVelocity = Service::bt2glm(p_landingSite->angularVelocity);
    //for(int i = 0; i < 9; i++)
    //    deltaToGround[i] = FINAL_APPROACH_DISTANCE;
}

void LanderAi::landerSimulationTick(btRigidBody* body, float timeStep){
    //setting to landing site rotation isnt right, 
    //need to construct new matrix that faces the landing site,

    if(autopilot){
        p_lander->rot = p_landingSite->rot;
        
        glm::mat4 scale = glm::scale(glm::mat4{ 1.0 }, p_lander->scale);
        glm::mat4 translation = glm::translate(glm::mat4{ 1.0 }, p_lander->pos);
        glm::mat4 rotation = p_lander->rot;
        p_lander->transformMatrix = translation * rotation * scale;
        //body->setWorldTransform(Service::glmToBulletT(p_lander->transformMatrix));
        //glm::mat4 look = glm::lookAt(p_lander->pos, p_landingSite->pos, p_lander->up);
        //p_lander->rot = look;
        p_lander->landerTransform.setBasis(Service::glmToBullet(p_lander->rot));
        body->setCenterOfMassTransform(Service::glmToBulletT(p_lander->transformMatrix));
    }

    /*if(touchdownBoost){
        //raycast the ground below
        //for now we cheat and just calculate to landing site, wont be as good but might indicate viability
        //
        float distanceToLandingSite = glm::length(p_landingSite->pos - p_lander->pos);
        std::cout << distanceToLandingSite << " dist to landing site\n";
        if (distanceToLandingSite < 2.0f){
            std::cout << "Boosting Reverse \n";
            //thrust oppositite vertical direction
            glm::vec3 reverse = glm::normalize(-p_lander->landerVelocityVector)*10.0f;
            p_lander->addImpulseToLanderQueue(1.0f, reverse.x, reverse.y, reverse.z, false);
            touchdownBoost = false;
            autopilot = false;
        }
            
    }*/
    
    if(imagingActive)
        imagingTimer(timeStep);
}

//this should be renamed, its more than an image timer
void LanderAi::imagingTimer(float timeStep){
    imagingTime += timeStep;
    //std::cout << imagingTime << " Image time\n";
    if(imagingTime > IMAGING_TIMER_SECONDS){
        //float remainder = imagingTime-IMAGING_TIMER_SECONDS;
        std::cout << "Image Requested\n";
        imagingTime = 0;
        //p_mediator->renderer_setShouldDrawOffscreen(true);

        if(autopilot){
            calculateMovement(timeStep);
            //calculateRotation();
        }
    }
}

void LanderAi::setRotation(){
    //glm::vec3 camPos = p_cameraData->cameraPos;
    //glm::mat4 view = glm::lookAt(camPos, camPos + p_cameraData->cameraFront, p_cameraData->cameraUp);
    p_lander->rot = p_landingSite->rot;
    glm::mat4 scale = glm::scale(glm::mat4{ 1.0 }, p_lander->scale);
    glm::mat4 translation = glm::translate(glm::mat4{ 1.0 }, p_lander->pos);
    glm::mat4 rotation = p_lander->rot;
    
    p_lander->transformMatrix = translation * rotation * scale;

    //glm::mat4 look = glm::lookAt(p_lander->pos, p_landingSite->pos, p_lander->up);
    //p_lander->rot = look;
    //m_Body->setCenterOfMassTransform(trans);
    //p_lander->landerTransform.setBasis(Service::glmToBullet(p_lander->rot));
}

void LanderAi::calculateRotation(){
    std::cout << glm::to_string(p_lander->landerAngularVelocity) << "\n";

    glm::vec3 desiredDirection = glm::normalize(p_lander->pos - p_landingSite->pos);

    std::cout << glm::to_string(desiredDirection) << "\n";

    glm::vec3 difference = desiredDirection - p_lander->landerAngularVelocity;

    glm::mat4 inv_transform = glm::inverse(p_lander->transformMatrix);
    difference = inv_transform * glm::vec4(difference, 0.0f);

    p_lander->addImpulseToLanderQueue(1.0f, difference.x, difference.y, difference.z, true);

    //glm::vec3 movementDirection = desiredPosition - p_lander->pos;


    /*p_lander->rot = p_landingSite->rot;

    glm::mat4 scale = glm::scale(glm::mat4{ 1.0 }, p_lander->scale);
    glm::mat4 translation = glm::translate(glm::mat4{ 1.0 }, p_lander->pos);
    glm::mat4 rotation = p_lander->rot;
    
    p_lander->transformMatrix = translation * rotation * scale;

    p_lander->landerTransform.setBasis(Service::glmToBullet(p_landingSite->rot));*/
}

//used in calculateMovement() to determine if we are above the landing site 
//and have been there for DESCENT_TICKER ticks
bool LanderAi::checkApproachAligned(float distanceToLandingSite){
    float angleFromUpVector = glm::angle(p_landingSite->up, glm::normalize(p_lander->pos - p_landingSite->pos));
    if(angleFromUpVector < 0.1f)
        if(distanceToLandingSite > minRange && distanceToLandingSite < maxRange){
            descentTicker+=1;
            if(descentTicker > DESCENT_TICKER)
                return true;
        }
    return false;
}

float LanderAi::getDistanceROC(){
    glm::vec3 groundUnderLanderPos = p_mediator->physics_performRayCast(p_lander->pos, -p_lander->up, 10000.0f); //raycast from lander down
    std::cout << glm::to_string(groundUnderLanderPos) << " ground under lander point\n";
    float distanceToGround = glm::length(groundUnderLanderPos - p_lander->pos);
    std::cout << distanceToGround << " distanceToGround\n";
    
    float roc = (distanceToGround/previousDistance - 1);

    //float distanceDelta = previousDistance - distanceToGround;
    //std::cout << distanceDelta << " distance delta\n";
    //distanceDelta -= FINAL_APPROACH_DISTANCE_REDUCTION;
    //std::cout << distanceDelta << " distance delta\n";
    previousDistance = distanceToGround;

    return roc;
}

float LanderAi::getAverageDistanceReduction(){
    //distancesToGround[]
    glm::vec3 groundUnderLanderPos = p_mediator->physics_performRayCast(p_lander->pos, -p_lander->up, 10000.0f); //raycast from lander down
    //std::cout << glm::to_string(groundUnderLanderPos) << " ground under lander point\n";
    float distanceToGround = glm::length(groundUnderLanderPos - p_lander->pos);
    //std::cout << distanceToGround << " distanceToGround\n";
    

    float distanceDelta = previousDistance - distanceToGround;
    //std::cout << distanceDelta << " distance delta\n";
    //distanceDelta -= FINAL_APPROACH_DISTANCE_REDUCTION;
    //std::cout << distanceDelta << " distance delta\n";
    previousDistance = distanceToGround;

    deltaToGround[distancesToGround_index] = distanceDelta;

    float total = 0;
    for(int i = 0; i < 9; i++)
        total += deltaToGround[i];

    distancesToGround_index++;
    if(distancesToGround_index > 9)
        distancesToGround_index = 0;

    return total/10;
}

void LanderAi::linearControl(float timeStep){
    //get the actual ground directly under the landing site object by raycasting down from the placeholder (more accurate this way)
    glm::vec3 landingSitePos = p_mediator->physics_performRayCast(p_landingSite->pos, -p_landingSite->up, 10.0f); //raycast from landing site past ground (ie -up*10)
    float distanceToLandingSite = glm::length(landingSitePos - p_lander->pos);

    //if we are not in the descent phase yet, call checkApproachAligned() 
    //to see if we are in the right position and have waited the right amount of time
    if(!shouldDescend)
        shouldDescend = checkApproachAligned(distanceToLandingSite);
    else{ 
        //calculate current vertical speed, how fast we are approaching the ground
        //float avgVerticalSpeed = getAverageDistanceReduction();

        //std::cout << avgVerticalSpeed << " avg vertical speed\n";

        //TODO should adjust approachDistance by the change in distance between ticks
        //this will allow us to catch up with an escaping ground or slow a rapidly approaching one
        /*float distanceDelta = 0;
        if(USE_DIST_DELTA_CORRECTION){

            glm::vec3 groundUnderLanderPos = p_mediator->physics_performRayCast(p_lander->pos, -p_lander->up, 10000.0f); //raycast from lander down
            std::cout << glm::to_string(groundUnderLanderPos) << " ground under lander point\n";
            float distanceToGround = glm::length(groundUnderLanderPos - p_lander->pos);
            std::cout << distanceToGround << " distanceToGround\n";

            if(previousDistance < 0){
                previousDistance = distanceToGround;
            }
            else{
                distanceDelta = previousDistance - distanceToGround;
                std::cout << distanceDelta << " distance delta\n";
                distanceDelta -= FINAL_APPROACH_DISTANCE_REDUCTION;
                std::cout << distanceDelta << " distance delta\n";
                previousDistance = distanceToGround;
            }
        }*/

        glm::vec3 groundUnderLanderPos = p_mediator->physics_performRayCast(p_lander->pos, -p_lander->up, 10000.0f); //raycast from lander down
        std::cout << glm::to_string(groundUnderLanderPos) << " ground under lander point\n";
        float distanceToGround = glm::length(groundUnderLanderPos - p_lander->pos);
        std::cout << distanceToGround << " distanceToGround\n";

        //if we have then we will begin to descend, we have two possible descent speeds
        //approachDistance is basically our target distance we want to be next cycle, 
        //starts at 50m above landing site, then we can reduce this distance each tick for controlled descent
        //APPROACH_DISTANCE_REDUCTION and FINAL_APPROACH_DISTANCE_REDUCTION are effectively vertical speed caps
        if(distanceToGround > FINAL_APPROACH_DISTANCE)
            approachDistance = distanceToGround-APPROACH_DISTANCE_REDUCTION;
        else{

            //if(avgVerticalSpeed - FINAL_APPROACH_DISTANCE_REDUCTION)
            ///float avgDistDelta = getAverageDistanceReduction();
            //std::cout << distanceDelta << " distance delta\n";
            //std::cout << avgDistDelta << " avg delta to ground\n";

            //float avgDistDelta = 0;

            //if(USE_DIST_DELTA_CORRECTION)
            //    avgDistDelta = getDistanceROC();
            //float avgDistDelta = getAverageDistanceReduction();

            //std::cout << avgDistDelta << " avg delta to ground\n";

            //glm::vec3 groundUnderLanderPos = p_mediator->physics_performRayCast(p_lander->pos, -p_lander->up, 10000.0f); //raycast from lander down
            //std::cout << glm::to_string(groundUnderLanderPos) << " ground under lander point\n";
            //float distanceToGround = glm::length(groundUnderLanderPos - p_lander->pos);
            float distanceDelta = getAverageDistanceReduction();//previousDistance - distanceToGround;
            //std::cout << distanceDelta << " distance delta\n";
            distanceDelta -= FINAL_APPROACH_DISTANCE_REDUCTION;
            //std::cout << distanceDelta << " distance delta\n";
            //previousDistance = distanceToGround;

            //approachDistance = distanceToGround-FINAL_APPROACH_DISTANCE_REDUCTION + (avgVerticalSpeed-FINAL_APPROACH_DISTANCE_REDUCTION);

            // ISSSUE --- This still isnt working correctly
            approachDistance = distanceToGround-FINAL_APPROACH_DISTANCE_REDUCTION + distanceDelta;
            //if(distanceDelta < 0)
            //    approachDistance += distanceDelta;
        }

        
    }

    //set an approach distance, starts at 50m, then reduces with each tick when shouldDescend is true
    glm::vec3 desiredDistance(approachDistance, approachDistance, approachDistance);

    //float distanceToLandingSite = glm::length(landingSitePos - p_lander->pos);
    //get the world position based on this desiredDistance and landingSite.up and position
    glm::vec3 desiredPosition = landingSitePos+(p_landingSite->up*desiredDistance);

    //get a movement vector from where lander is to where we want it to be
    glm::vec3 desiredMovement = desiredPosition - p_lander->pos;
    
    //we can cap it by a LANDER_SPEED_CAP to keep it from gaining too much speed
    if(glm::length(desiredMovement) > LANDER_SPEED_CAP)
        desiredMovement = glm::normalize(desiredMovement) * LANDER_SPEED_CAP;   
    
    //desiredMovement is the ideal velocity vector for our next physics cycle, but we need to account for current velocity
    //calculate the force needed to achieve desiredMovement vector by subtracting landerVelocityVector
    glm::vec3 correctedMovement = desiredMovement - p_lander->landerVelocityVector;

    //before submitting to the movement queue we will adjust this vector to be relative to lander orientation
    //to do so we take the inverse of the transformation matrix and apply to the desired movement vector to 
    //rotate it to the relative orientation, we do this just for any machine learning algorithms benefit
    glm::mat4 inv_transform = glm::inverse(p_lander->transformMatrix);
    correctedMovement = inv_transform * glm::vec4(correctedMovement, 0.0f);
    
    //finally submit it to the queue, TODO!!! we will probably need to normalize vector and pass a strength
    p_lander->addImpulseToLanderQueue(1.0f, correctedMovement.x, correctedMovement.y, correctedMovement.z, false);

    //TODO write some output, will need to be timestamped and linked to images somehow. filename probably
    //if(outputActionToFile)
}

float tf = 1000.0f;
float t = 0.0f;
float tgo = 1000.0f;

glm::vec3 LanderAi::getZEM(glm::vec3 rf, glm::vec3 r, glm::vec3 v){
    std::cout << glm::to_string(rf) << " rf\n";
    std::cout << glm::to_string(r) << " r\n";
    float ix = 0.5 * (tgo * tgo) * p_lander->landerGravityVector.getX(); //this is not integral
    float x = rf.x - (r.x + (tgo*v.x) + ix);
    
    float iy = 0.5 * (tgo * tgo) * p_lander->landerGravityVector.getY(); //this is not integral
    float y = rf.y - (r.y + (tgo*v.y) + iy);

    float iz = 0.5 * (tgo * tgo) * p_lander->landerGravityVector.getZ(); //this is not integral
    float z = rf.z - (r.z + (tgo*v.z) + iz);

    return glm::vec3(x,y,z);
}

glm::vec3 LanderAi::getZEV(glm::vec3 vf, glm::vec3 v){
    float ix = tgo * p_lander->landerGravityVector.getX(); //this is not integral
    float x = vf.x - (v.x + ix);

    float iy = tgo * p_lander->landerGravityVector.getY(); //this is not integral
    float y = vf.y - (v.y + iy);

    float iz = tgo * p_lander->landerGravityVector.getZ(); //this is not integral
    float z = vf.z - (v.z + iz);

    return glm::vec3(x,y,z);
}

glm::vec3 LanderAi::getZEMZEVAccel(glm::vec3 zem, glm::vec3 zev){
    float x =  (6/(tgo*tgo))*zem.x - (2/tgo*zev.x);
    float y =  (6/(tgo*tgo))*zem.y - (2/tgo*zev.y);
    float z =  (6/(tgo*tgo))*zem.z - (2/tgo*zev.z);
    return glm::vec3(x,y,z);
}

glm::vec3 LanderAi::getZEMZEVa(glm::vec3 rf, glm::vec3 r, glm::vec3 v, glm::vec3 w, float radius){
    float x =  6/(tgo*tgo)*(rf.x - r.x) + ((4/tgo)*v.x + 2*w.x * v.x + w.x * (w.x*radius));
    float y =  6/(tgo*tgo)*(rf.y - r.y) + ((4/tgo)*v.y + 2*w.y * v.y + w.y * (w.y*radius));
    float z =  6/(tgo*tgo)*(rf.z - r.z) + ((4/tgo)*v.z + 2*w.z * v.z + w.z * (w.z*radius));

    return glm::vec3(x,y,z);
}


void LanderAi::ZEM_ZEV_Control(float timeStep){
    //ISSUE need to provide some waypoint system maybe?
    //or slide the vertical point distance by subtracting log d
    //would need to adjust tgo as well
    //

    glm::vec3 desiredPosition;
    glm::vec3 desiredDistance(approachDistance, approachDistance, approachDistance);
    glm::vec3 landingSitePos = p_mediator->physics_performRayCast(p_landingSite->pos, -p_landingSite->up, 10.0f); //raycast from landing site past ground (ie -up*10)
    
    //calculate time to go
    t+=IMAGING_TIMER_SECONDS; //cant just be timestep, must be imaging time?
    if(!shouldDescend){
        desiredPosition = landingSitePos+(p_landingSite->up*desiredDistance); //get the world position based on this desiredDistance and landingSite.up and position
        float remainingTime = glm::length(desiredPosition - p_lander->pos) / p_lander->landerVelocity; //t = d/v
        std::cout << remainingTime  << " remainingTime-to-go\n";
        if(remainingTime < 10.0f){
            approachDistance = 0.0f;
            shouldDescend = true;
            tf = 1000.0f;
            t = 0;
        }
        else
            approachDistance = 500.0f;
    }
    else{
        desiredPosition = landingSitePos;
    }

    tgo = tf - t;//this is not a true tgo value, need to actually calculate an interception based on relative speeds and positions
    if(tgo < 0){
        tgo = 1000.0f;
        tf = 1000.0f;
        t = 0.0f;
    }
    std::cout << tgo  << " time-to-go\n";

    glm::vec3 rf = desiredPosition;
    //glm::vec3 rf = glm::vec3(0.0f);
    //glm::vec3 r = p_landingSite->pos - p_lander->pos;
    glm::vec3 r = p_lander->pos;
    //glm::vec3 r = p_lander->pos - p_landingSite->pos ;
    glm::vec3 zem = getZEM(rf, r, p_lander->landerVelocityVector);

    //first param is landing site velocity, or is it relative velocity? for now we use 0
    //2nd param is relative velocity, landing site vel - lander vel
    //glm::vec3 lsVelocity = asteroidAngularVelocity;
    glm::vec3 lsVelocity = asteroidAngularVelocity * glm::length(landingSitePos);
    //glm::vec3 zev = getZEV(glm::vec3(0.0f), p_lander->landerVelocityVector);
    glm::vec3 zev = getZEV(lsVelocity, p_lander->landerVelocityVector);

    glm::vec3 acc = getZEMZEVAccel(zem,zev);
    //glm::vec3 acc = getZEMZEVa(rf,r,p_lander->landerVelocityVector, asteroidAngularVelocity);
    std::cout << glm::to_string(asteroidAngularVelocity) << " angular vel \n";
    std::cout << glm::to_string(acc) << " acc \n";

    glm::mat4 inv_transform = glm::inverse(p_lander->transformMatrix);
    glm::vec3 correctedMovement = inv_transform * glm::vec4(acc, 0.0f);
    
    //finally submit it to the queue, TODO!!! we will probably need to normalize vector and pass a strength
    p_lander->addImpulseToLanderQueue(1.0f, correctedMovement.x, correctedMovement.y, correctedMovement.z, false);

    //p_lander->addImpulseToLanderQueue(1.0f, acc.x, acc.y, acc.z, false);
}

//function uses world state to calculate movement vector to control Lander automatically
//will form the basis of generating lots of training data rapidly
//will need to output data to file, along with images or any other data for training
void LanderAi::calculateMovement(float timeStep){
    //linearControl(timeStep);
    ZEM_ZEV_Control(timeStep);
}

void LanderAi::setAutopilot(bool b){
    autopilot = b;
}

void LanderAi::setImaging(bool b){
    imagingActive = b;
}