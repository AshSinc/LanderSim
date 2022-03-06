#include "lander_gnc.h"
#include "mediator.h"
#define GLM_FORCE_RADIANS //makes sure GLM uses radians to avoid confusion
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES //forces GLM to use a version of vec2 and mat4 that have the correct alignment requirements for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE //forces GLM to use depth range of 0 to 1, instead of -1 to 1 as in OpenGL
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/norm.hpp> //for glm::length2
#include <glm/gtx/vector_angle.hpp> //for vectorangle
#include <iostream>

//used for debug drawing only, can make a much better debug drawing method but need more time to set up renderer
#include "obj_render.h"
#include <memory>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

using namespace Lander;

void GNC::init(Mediator* mediator, NavigationStruct* gncVars){
    p_mediator = mediator; //mediator only used for debug drawing
    p_navStruct = gncVars;
}

//check if we are above the expected future landing site position
bool GNC::checkApproachAligned(glm::vec3 futureLsUp, glm::vec3 futureLsPos){
    glm::vec3 landerPos = glm::vec3(p_navStruct->landerTransformMatrix[3]); //extract position vector form translation component of transform matrix
    float angleFromUpVector = glm::angle(futureLsUp, glm::normalize(landerPos - futureLsPos));
    std::cout << angleFromUpVector << " current angle \n";
    if(angleFromUpVector < APPROACH_ANGLE){
        return true;
    }
    return false;
}

glm::vec3 GNC::getZEM(glm::vec3 rf, glm::vec3 r, glm::vec3 v){
    float ix = 0.5 * (tgo * tgo) * p_navStruct->gravityVector.x; //this is not integral
    float x = rf.x - (r.x + (tgo*v.x) + ix);
    float iy = 0.5 * (tgo * tgo) * p_navStruct->gravityVector.y; //this is not integral
    float y = rf.y - (r.y + (tgo*v.y) + iy);
    float iz = 0.5 * (tgo * tgo) * p_navStruct->gravityVector.z; //this is not integral
    float z = rf.z - (r.z + (tgo*v.z) + iz);
    return glm::vec3(x,y,z);
}

glm::vec3 GNC::getZEV(glm::vec3 vf, glm::vec3 v){
    float ix = tgo * p_navStruct->gravityVector.x; //this is not integral
    float x = vf.x - (v.x + ix);
    float iy = tgo * p_navStruct->gravityVector.y; //this is not integral
    float y = vf.y - (v.y + iy);
    float iz = tgo * p_navStruct->gravityVector.z; //this is not integral
    float z = vf.z - (v.z + iz);
    return glm::vec3(x,y,z);
}

glm::vec3 GNC::getZEMZEVAccel(glm::vec3 zem, glm::vec3 zev){
    float x =  (6/(tgo*tgo))*zem.x - (2/tgo*zev.x);
    float y =  (6/(tgo*tgo))*zem.y - (2/tgo*zev.y);
    float z =  (6/(tgo*tgo))*zem.z - (2/tgo*zev.z);
    return glm::vec3(x,y,z);
}

void GNC::updateTgo(float timeStep){
    t+= timeStep;
    tgo = tf - t;//this is not a true tgo value, need to actually calculate an interception based on relative speeds and positions
    if(tgo < 0){
        tgo = TF_TIME;
        tf = TF_TIME;
        t = 0.0f;
    }
    std::cout << tgo  << " time-to-go\n";
}

glm::vec3 GNC::stabiliseCurrentPos(){
    glm::mat4 inv_transform = glm::inverse(p_navStruct->landerTransformMatrix);
    glm::vec3 correctedMovement = inv_transform * glm::vec4(-p_navStruct->velocityVector, 0.0f);
    return correctedMovement;
}

//preApproach routine
//holds the lander steady and checks for a good time to start descent
glm::vec3 GNC::preApproach(){
    //work out where the landing site is and where is "up" at the maximum time for approach
    if(p_navStruct->estimationComplete){
        calculateVectorsAtTime(TF_TIME);

        //ISSUE - TODO we should add a check in case we are never going to be in 15 degrees alignment
        //called alignment stage, that will reposition to the nearest point that will be in alignment
        //then check if we are above that point, within 15 degrees
        //maybe check if the alignment angle is reducing and then increasing, once starts to increase move to the current alignment position and recheck
        //could use zem zev again but would need to add a new stage and possibly alter current inputs?
        if(checkApproachAligned(projectedLandingSiteUp, projectedLandingSitePos)){
            shouldDescend = true;
        }
    }

    //return a vector that will stabilize movement to 0
    return stabiliseCurrentPos();
}

glm::mat4 GNC::constructRotationMatrixAtTf(float ttgo){
    //get degrees of rotation per second from asteroidAngularVelocity multiply by tgo to get expected total rotation
    float degreesMovedAtTgo = glm::length(p_navStruct->angularVelocity)*ttgo;

    //construct rotated matrix
    return glm::mat3(glm::rotate(glm::mat4(1.0f), degreesMovedAtTgo, glm::normalize(p_navStruct->angularVelocity)));
}

glm::vec3 GNC::predictFinalLandingSitePos(glm::mat4 rotationM){
    //rotate landingsite pos by the expected rotation
    return rotationM * glm::vec4(p_navStruct->landingSitePos, 1.0f);
}

glm::vec3 GNC::predictFinalLandingSiteUp(glm::mat4 rotationM){
    //rotate landingsite up by the expected rotation
    return rotationM * glm::vec4(p_navStruct->landingSiteUp, 0.0f);
}

void GNC::calculateVectorsAtTime(float time){
    if(glm::length(p_navStruct->angularVelocity) != 0){ //if angular vel is 0 we dont need to check
        rotationMatrixAtTf = constructRotationMatrixAtTf(time);
        //std::cout << glm::to_string(rotationMatrixAtTf) << "Calculated rotation\n";
        projectedLandingSitePos = predictFinalLandingSitePos(rotationMatrixAtTf);
        projectedLandingSiteUp = predictFinalLandingSiteUp(rotationMatrixAtTf);
        glm::mat4 rotationMatrixAtTfPlus1 = constructRotationMatrixAtTf(time+1);
        glm::vec3 projectedLandingSitePosPlus1 = predictFinalLandingSitePos(rotationMatrixAtTfPlus1);
        projectedVelocityAtTf = projectedLandingSitePosPlus1 - projectedLandingSitePos; //must be a cleaner way to get linear vel of point in 3d from angular velocity, this is a workaround
    }
    else{
        //even if no rotation lets just set them for ease
        projectedLandingSitePos = p_navStruct->landingSitePos;
        projectedLandingSiteUp = p_navStruct->landingSiteUp;
        projectedVelocityAtTf = glm::vec3(0,0,0);
    }

    //if debug (p_mediator->showDebugging())
    //or a #define DEBUG_DRAWING somewhere would probably be better
    std::vector<std::shared_ptr<RenderObject>>* p_debugObjects = p_mediator->scene_getDebugObjects();
    p_debugObjects->at(0).get()->pos = projectedLandingSitePos;
    p_debugObjects->at(1).get()->pos = projectedLandingSitePos + projectedLandingSiteUp;
    p_debugObjects->at(2).get()->pos = projectedLandingSitePos + projectedVelocityAtTf;
}

glm::vec3 GNC::ZEM_ZEV_Control(float timeStep){
    updateTgo(timeStep);

    //calculate projectedLandingSitePos and projectedVelocityAtTf
    calculateVectorsAtTime(tgo);

    glm::vec3 rf = projectedLandingSitePos;
    glm::vec3 r = glm::vec3(p_navStruct->landerTransformMatrix[3]); //extract landing site pos from third column of transform matrix

    glm::vec3 zem = getZEM(rf, r, p_navStruct->velocityVector);
    glm::vec3 zev = getZEV(projectedVelocityAtTf, p_navStruct->velocityVector);
    glm::vec3 acc = getZEMZEVAccel(zem,zev);

    glm::mat4 inv_transform = glm::inverse(p_navStruct->landerTransformMatrix);
    glm::vec3 correctedMovement = inv_transform * glm::vec4(acc, 0.0f);
    
    return correctedMovement;
}

glm::vec3 GNC::getThrustVector(float timeStep){
    glm::vec3 thrustVector{0};
    if(!shouldDescend)
        thrustVector = preApproach();
    else
        thrustVector = ZEM_ZEV_Control(timeStep); //should be called zemzev approach or something

    return thrustVector;
}

//http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-17-quaternions/#how-do-i-use-lookat-but-limit-the-rotation-at-a-certain-speed-
//example here, a slerp catching common issues
/*glm::quat GNC::rotateTowards(glm::quat q1, glm::quat q2, float maxAngle){
	if( maxAngle < 0.001f ){
		// No rotation allowed. Prevent dividing by 0 later.
		return q1;
	}

	float cosTheta = dot(q1, q2);

	// q1 and q2 are already equal.
	// Force q2 just to be sure
	if(cosTheta > 0.9999f){
		return q2;
	}

	// Avoid taking the long path around the sphere
	if (cosTheta < 0){
	    q1 = q1*-1.0f;
	    cosTheta *= -1.0f;
	}

	float angle = glm::acos(cosTheta);

	// If there is only a 2&deg; difference, and we are allowed 5&deg;,
	// then we arrived.
	if (angle < maxAngle){
		return q2;
	}

	float fT = maxAngle / angle;
	angle = maxAngle;

	glm::quat res = (glm::sin((1.0f - fT) * angle) * q1 + glm::sin(fT * angle) * q2) / glm::sin(angle);
	res = normalize(res);
	return res;

}*/

glm::vec3 GNC::getProjectedUpVector(){
    return projectedLandingSiteUp;
}


    //initialisation
    //minRange = approachDistance - 5;
    //maxRange = approachDistance + 5;
    //previousDistance = FINAL_APPROACH_DISTANCE;

/*void CPU::linearControl(float timeStep){
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
    addImpulseToLanderQueue(1.0f, correctedMovement.x, correctedMovement.y, correctedMovement.z, false);
}*/