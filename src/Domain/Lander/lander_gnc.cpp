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

glm::vec3 GNC::getThrustVector(float timeStep){
    glm::vec3 thrustVector{0};

    glm::vec3 sitePos, siteUp, angularVelocity;

    if(p_navStruct->useOnlyEstimate){
        sitePos = p_navStruct->landingSitePos_Estimate;
        siteUp = p_navStruct->landingSiteUp_Estimate;
        angularVelocity = p_navStruct->angularVelocityOfAsteroid_Estimate;
    }
    else{
        sitePos = p_navStruct->landingSitePos;
        siteUp = p_navStruct->landingSiteUp;
        angularVelocity = p_navStruct->angularVelocityOfAsteroid;
    }

    if(!shouldDescend)
        thrustVector = preApproach(sitePos, siteUp, angularVelocity);
    else{
        thrustVector = ZEM_ZEV_Control(timeStep, sitePos, siteUp, angularVelocity);

        if(Service::OUTPUT_TEXT){
            //output nav data to file
            std::string time = std::to_string(p_mediator->physics_getTimeStamp());
            std::string text = time + ":" + glm::to_string(p_navStruct->landerPos);
            p_mediator->writer_writeToFile("NAV", text);

            text = time + ":" + glm::to_string(thrustVector);
            p_mediator->writer_writeToFile("THRUST", text);
        }
    }

    return thrustVector;
}

//preApproach routine
//holds the lander steady and checks for a good time to start descent
glm::vec3 GNC::preApproach(glm::vec3 sitePos, glm::vec3 siteUp, glm::vec3 angularVelocity){
    //work out where the landing site is and where is "up" at the maximum time for approach
    if(p_navStruct->estimationComplete){
        calculateVectorsAtTime(TF_TIME, sitePos, siteUp, angularVelocity);
        shouldDescend = checkApproachAligned(projectedLandingSiteUp, projectedLandingSitePos);
    }
    //return a vector that will stabilize movement to 0
    return stabiliseCurrentPos();
}

//check if we are above the expected future landing site position
bool GNC::checkApproachAligned(glm::vec3 futureLsUp, glm::vec3 futureLsPos){
    float angleFromUpVector = glm::angle(futureLsUp, glm::normalize(p_navStruct->landerPos - futureLsPos));
    
    std::cout << angleFromUpVector << " current angle from up vector \n";
    
    if(angleFromUpVector < APPROACH_ANGLE)
        return true;
    return false;
}

void GNC::calculateVectorsAtTime(float time, glm::vec3 sitePos, glm::vec3 siteUp, glm::vec3 angularVelocity){
    if(glm::length(p_navStruct->angularVelocityOfAsteroid) != 0){ //if angular vel is 0 we dont need to check
        //calculation
        rotationMatrixAtTf = constructRotationMatrixAtTf(time, angularVelocity);
        projectedLandingSitePos = rotationMatrixAtTf * glm::vec4(sitePos, 1.0f);
        projectedLandingSiteUp = rotationMatrixAtTf * glm::vec4(siteUp, 1.0f);
        glm::mat4 rotationMatrixAtTfPlus1 = constructRotationMatrixAtTf(time+1, angularVelocity);
        glm::vec3 projectedLandingSitePosPlus1 = rotationMatrixAtTfPlus1 * glm::vec4(sitePos, 1.0f);
        projectedVelocityAtTf = projectedLandingSitePosPlus1 - projectedLandingSitePos; //there's a cleaner way to get linear vel of point in 3d from angular velocity, this is a workaround

        if(Service::OUTPUT_TEXT){
            //output nav data to file
            if(!shouldDescend){ 
                std::string time = std::to_string(p_mediator->physics_getTimeStamp());
                std::string text = time + ":projectedposition:" + glm::to_string(projectedLandingSitePos);
                p_mediator->writer_writeToFile("PRE", text);
            }
        }

        if(p_navStruct->useOnlyEstimate)
            moveEstimatedLandingSiteForward1Second();
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

glm::mat4 GNC::constructRotationMatrixAtTf(float ttgo, glm::vec3 angularVelocity){
    //get degrees of rotation per second from asteroidAngularVelocity multiply by tgo to get expected total rotation
    float degreesMovedAtTgo = glm::length(angularVelocity)*ttgo;

    //construct rotated matrix
    return glm::mat3(glm::rotate(glm::mat4(1.0f), degreesMovedAtTgo, glm::normalize(angularVelocity)));
}

glm::vec3 GNC::ZEM_ZEV_Control(float timeStep, glm::vec3 sitePos, glm::vec3 siteUp, glm::vec3 angularVelocity){
    updateTgo(timeStep);

    //calculate projectedLandingSitePos and projectedVelocityAtTf
    calculateVectorsAtTime(tgo, sitePos, siteUp, angularVelocity);

    glm::vec3 zem = getZEM(projectedLandingSitePos, p_navStruct->landerPos, p_navStruct->velocityVector);
    glm::vec3 zev = getZEV(projectedVelocityAtTf, p_navStruct->velocityVector);
    glm::vec3 acc = getZEMZEVAccel(zem,zev);

    if(Service::OUTPUT_TEXT){
        //output nav data to file
        std::string time = std::to_string(p_mediator->physics_getTimeStamp());
        std::string text = time + ":ZEM:" + glm::to_string(zem);
        p_mediator->writer_writeToFile("GNC", text);

        text = time + ":ZEV:" + glm::to_string(zev);
        p_mediator->writer_writeToFile("GNC", text);

        text = time + ":ZEMZEV_Accel:" + glm::to_string(acc);
        p_mediator->writer_writeToFile("GNC", text);
    }

    glm::mat4 inv_transform = glm::inverse(p_navStruct->landerTransformMatrix);
    glm::vec3 correctedMovement = inv_transform * glm::vec4(acc, 0.0f);
    
    return correctedMovement;
}

glm::vec3 GNC::getZEM(glm::vec3 rf, glm::vec3 r, glm::vec3 v){
    float ix = 0.5 * (tgo * tgo) * p_navStruct->gravityVector.x;
    float x = rf.x - (r.x + (tgo*v.x) + ix);
    float iy = 0.5 * (tgo * tgo) * p_navStruct->gravityVector.y;
    float y = rf.y - (r.y + (tgo*v.y) + iy);
    float iz = 0.5 * (tgo * tgo) * p_navStruct->gravityVector.z;
    float z = rf.z - (r.z + (tgo*v.z) + iz);
    return glm::vec3(x,y,z);
}

glm::vec3 GNC::getZEV(glm::vec3 vf, glm::vec3 v){
    float ix = tgo * p_navStruct->gravityVector.x;
    float x = vf.x - (v.x + ix);
    float iy = tgo * p_navStruct->gravityVector.y;
    float y = vf.y - (v.y + iy);
    float iz = tgo * p_navStruct->gravityVector.z;
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

//used for estimate only calculations
//basically we need the current position every second, so we have this new method that only uses the estimation instead of real values
void GNC::moveEstimatedLandingSiteForward1Second(){
    //get degrees of rotation per second from asteroidAngularVelocity
    float degreesMovedIn1S = glm::length(p_navStruct->angularVelocityOfAsteroid_Estimate);
    glm::mat3 rotation_in_1s = glm::mat3(glm::rotate(glm::mat4(1.0f), degreesMovedIn1S, glm::normalize(p_navStruct->angularVelocityOfAsteroid_Estimate)));

    p_navStruct->landingSitePos_Estimate = rotation_in_1s * p_navStruct->landingSitePos_Estimate;
    p_navStruct->landingSiteUp_Estimate = rotation_in_1s * p_navStruct->landingSiteUp_Estimate; //we dont actually need up once descent has started...
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