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
}

void LanderAi::landerSimulationTick(btRigidBody* body, float timeStep){
    //setting to landing site rotation isnt right, 
    //need to construct new matrix that faces the landing site,

    if(autopilot){
        setRotation(); //we are cheating and locking rotation to landing s
        body->setCenterOfMassTransform(Service::glmToBulletT(p_lander->transformMatrix));
    }
    
    if(imagingActive)
        imagingTimer(timeStep);
}

//this should be renamed, its more than an image timer
void LanderAi::imagingTimer(float timeStep){
    imagingTime += timeStep;
    if(imagingTime > IMAGING_TIMER_SECONDS){
        std::cout << "Image Requested\n";
        imagingTime = 0;
        //p_mediator->renderer_setShouldDrawOffscreen(true);

        if(autopilot){
            if(!shouldDescend)
                preApproach();
            else
                ZEM_ZEV_Control(timeStep); //should be called zemzev approach or something
            //calculateRotation();
        }
    }
}

void LanderAi::setRotation(){
    p_lander->rot = p_landingSite->rot;
    glm::mat4 scale = glm::scale(glm::mat4{ 1.0 }, p_lander->scale);
    glm::mat4 translation = glm::translate(glm::mat4{ 1.0 }, p_lander->pos);
    glm::mat4 rotation = p_lander->rot;
    p_lander->transformMatrix = translation * rotation * scale;
    p_lander->landerTransform.setBasis(Service::glmToBullet(p_lander->rot));
}

void LanderAi::calculateRotation(){
    std::cout << glm::to_string(p_lander->landerAngularVelocity) << "\n";
    glm::vec3 desiredDirection = glm::normalize(p_lander->pos - p_landingSite->pos);
    std::cout << glm::to_string(desiredDirection) << "\n";
    glm::vec3 difference = desiredDirection - p_lander->landerAngularVelocity;
    glm::mat4 inv_transform = glm::inverse(p_lander->transformMatrix);
    difference = inv_transform * glm::vec4(difference, 0.0f);
    p_lander->addImpulseToLanderQueue(1.0f, difference.x, difference.y, difference.z, true);
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

//check if we are above the expected future landing site position
bool LanderAi::checkApproachAligned(glm::vec3 futureLsUp, glm::vec3 futureLsPos){
    float angleFromUpVector = glm::angle(futureLsUp, glm::normalize(p_lander->pos - futureLsPos));
    std::cout << angleFromUpVector << " current angle \n";
    if(angleFromUpVector < APPROACH_ANGLE){
        return true;
    }
    return false;
}

float LanderAi::getAverageDistanceReduction(){
    glm::vec3 groundUnderLanderPos = p_mediator->physics_performRayCast(p_lander->pos, -p_lander->up, 10000.0f); //raycast from lander down
    float distanceToGround = glm::length(groundUnderLanderPos - p_lander->pos);
    float distanceDelta = previousDistance - distanceToGround;
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

            float distanceDelta = getAverageDistanceReduction();//previousDistance - distanceToGround;
            distanceDelta -= FINAL_APPROACH_DISTANCE_REDUCTION;

            // ISSSUE --- This still isnt working correctly
            approachDistance = distanceToGround-FINAL_APPROACH_DISTANCE_REDUCTION + distanceDelta;
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
}

glm::vec3 LanderAi::getZEM(glm::vec3 rf, glm::vec3 r, glm::vec3 v){
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

void LanderAi::updateTgo(){
    t+=IMAGING_TIMER_SECONDS; //cant just be timestep, must be imaging time?
    tgo = tf - t;//this is not a true tgo value, need to actually calculate an interception based on relative speeds and positions
    if(tgo < 0){
        tgo = 1000.0f;
        tf = 1000.0f;
        t = 0.0f;
    }
    std::cout << tgo  << " time-to-go\n";
}

void LanderAi::stabiliseCurrentPos(){
    glm::mat4 inv_transform = glm::inverse(p_lander->transformMatrix);
    glm::vec3 correctedMovement = inv_transform * glm::vec4(-p_lander->landerVelocityVector, 0.0f);
    p_lander->addImpulseToLanderQueue(1.0f, correctedMovement.x, correctedMovement.y, correctedMovement.z, false);
}

void LanderAi::preApproach(){
    //stabilise at distance, just counteract current accel while we do initial imageing, need to probably have a bool to check for stages, or a state represented by int
    stabiliseCurrentPos();

    //perform imaging

    //process waiting images

    //once processed only then should we do the next part, maybe move to another method   

    //if we are not on the right axis alignment, we should probably move into it somehow
    //maybe wait a perioid of time and then move?

    //or.. calculate if we are on the axis of travel of landing site first, 
    //then move to position
    //then check for the next descent part

    calculateVectorsAtTime(tgo);

    if(checkApproachAligned(projectedLandingSiteUp, projectedLandingSitePos)){
        shouldDescend = true;
    }
}

glm::mat4 LanderAi::constructRotationMatrixAtTf(float ttgo){
    //get degrees of rotation per second from asteroidAngularVelocity multiply by tgo to get expected total rotation
    float degreesMovedAtTgo = glm::length(asteroidAngularVelocity)*ttgo;
    //construct rotated matrix
    return glm::rotate(glm::mat4(1.0f), degreesMovedAtTgo, glm::normalize(asteroidAngularVelocity));
}

glm::vec3 LanderAi::predictFinalLandingSitePos(glm::mat4 rotationM){
    glm::vec3 landingSitePos = p_mediator->physics_performRayCast(p_landingSite->pos, -p_landingSite->up, 10.0f); //raycast from landing site past ground (ie -up*10)
    //rotate landingsite pos by the expected rotation
    return rotationM * glm::vec4(landingSitePos, 1.0f);
}

glm::vec3 LanderAi::predictFinalLandingSiteUp(glm::mat4 rotationM){
    //rotate landingsite up by the expected rotation
    return rotationM * glm::vec4(p_landingSite->up, 0.0f);
}

void LanderAi::calculateVectorsAtTime(float time){
    if(glm::length(asteroidAngularVelocity) != 0){ //if angular vel is 0 we dont need to check
        glm::mat4 rotationMatrixAtTf = constructRotationMatrixAtTf(time);
        projectedLandingSitePos = predictFinalLandingSitePos(rotationMatrixAtTf);
        projectedLandingSiteUp = predictFinalLandingSiteUp(rotationMatrixAtTf);
        glm::mat4 rotationMatrixAtTfPlus1 = constructRotationMatrixAtTf(time+1);
        glm::vec3 projectedLandingSitePosPlus1 = predictFinalLandingSitePos(rotationMatrixAtTfPlus1);
        projectedVelocityAtTf = projectedLandingSitePosPlus1 - projectedLandingSitePos; //must be a cleaner way to get linear vel of point in 3d from angular velocity, this is a workaround
        //if debug (p_mediator->showDebugging())
        p_lander->p_debugBox1->pos = projectedLandingSitePos;
        p_lander->p_debugBox2->pos = projectedLandingSitePos+projectedLandingSiteUp;
        p_lander->p_debugBox3->pos = projectedLandingSitePos + projectedVelocityAtTf;
    }
    else{
        //even if no rotation lets just set them for ease
        projectedLandingSitePos = p_landingSite->pos;
        projectedLandingSiteUp = p_landingSite->up;
        projectedVelocityAtTf = glm::vec3(0,0,0);
    }
}

void LanderAi::ZEM_ZEV_Control(float timeStep){
    //ISSUE need to provide some waypoint system maybe?
    //or slide the vertical point distance by subtracting log d
    //would need to adjust tgo as well

    updateTgo();

    //calculate projectedLandingSitePos and projectedVelocityAtTf
    calculateVectorsAtTime(tgo);

    glm::vec3 rf = projectedLandingSitePos;
    glm::vec3 r = p_lander->pos;

    glm::vec3 zem = getZEM(rf, r, p_lander->landerVelocityVector);
    glm::vec3 zev = getZEV(projectedVelocityAtTf, p_lander->landerVelocityVector);
    glm::vec3 acc = getZEMZEVAccel(zem,zev);

    glm::mat4 inv_transform = glm::inverse(p_lander->transformMatrix);
    glm::vec3 correctedMovement = inv_transform * glm::vec4(acc, 0.0f);
    
    //submit it to the queue
    p_lander->addImpulseToLanderQueue(1.0f, correctedMovement.x, correctedMovement.y, correctedMovement.z, false);
}

//function uses world state to calculate movement vector to control Lander automatically
//will form the basis of generating lots of training data rapidly
//will need to output data to file, along with images or any other data for training
void LanderAi::calculateMovement(float timeStep){
    //linearControl(timeStep);
    //before doing this we need to check it will be facing us at tgo
    ZEM_ZEV_Control(timeStep);
}

void LanderAi::setAutopilot(bool b){
    autopilot = b;
}

void LanderAi::setImaging(bool b){
    imagingActive = b;
}