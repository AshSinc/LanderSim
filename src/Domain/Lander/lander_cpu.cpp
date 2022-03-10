#include "lander_cpu.h"
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

using namespace Lander;

void CPU::init(Mediator* mediator, LanderObj* lander){
    p_mediator = mediator;
    p_lander = lander;
    p_landingSite = p_mediator->scene_getLandingSiteObject();
    asteroidAngularVelocity = Service::bt2glm(p_landingSite->angularVelocity);

    navStruct.useOnlyEstimate = lander->useEstimateOnly; //setting here instead of rewritting heirarchy, crunch time
    gnc.init(mediator, &navStruct);

    navStruct.asteroidScale = lander->asteroidScale;
    navStruct.angularVelocityOfAsteroid = asteroidAngularVelocity;
    //set renderer fov
    p_mediator->renderer_setOpticsFov(BASE_OPTICS_FOV*lander->asteroidScale);
    
    cv.init(mediator, IMAGING_TIMER_SECONDS, &navStruct);

    if(Service::OUTPUT_TEXT){
        //output fov
        p_mediator->writer_writeToFile("PARAMS", "FOV:" + std::to_string(BASE_OPTICS_FOV*lander->asteroidScale));
        p_mediator->writer_writeToFile("PARAMS", "IMAGE_TIMER:" + std::to_string(IMAGING_TIMER_SECONDS));
        p_mediator->writer_writeToFile("PARAMS", "GNC_TIMER:" + std::to_string(GNC_TIMER_SECONDS));
    }
}

void CPU::simulationTick(btRigidBody* body, float timeStep){
    try{
        cv.simulationTick(); //let vision check for image avaiable from renderer
    }
    catch(std::runtime_error& e){
        std::cout << e.what() << "\n";
    }

    //reaction wheel slew code needs completed
    if(reactionWheelEnabled){
        //body->setCenterOfMassTransform(Service::glmToBulletT(p_lander->transformMatrix));
        //slewToLandingSiteOrientation();
    }
    
    //here we will compute distance to asteroid for calibrating optics fov zoom
    //and then inform offscreen renderer that it can draw the image next cpu cycle
    if(imagingTimer(timeStep)){

        //first check distance to center point of our camera in world space and store in navstruct to share with gnc and vision
        glm::vec3 opticsCenterWorldPoint = p_mediator->physics_performRayCast(p_lander->pos, -p_lander->up, 100000.0f);
        navStruct.altitude = glm::length(p_lander->pos-opticsCenterWorldPoint);
        navStruct.radiusAtOpticalCenter = glm::length(opticsCenterWorldPoint);
        std::cout << glm::to_string(p_lander->pos) << " lander pos \n";
        std::cout << navStruct.altitude << " distanceToSurface \n";

        p_mediator->renderer_setShouldDrawOffscreen(true); //inform renderer that it can draw offscreen image next cpu cycle
    }

    
    if(gncTimer(timeStep)){
        //store real positions
        navStruct.landerPos = p_lander->pos;
        navStruct.landingSitePos = p_mediator->physics_performRayCast(p_landingSite->pos, -p_landingSite->up, 10.0f); //raycast from landing site past ground (ie -up*10)
        navStruct.landingSiteUp = p_landingSite->up;

        
        if(cv.active == false){
            if(navStruct.useOnlyEstimate && estimateComplete == false){
                showEstimationStats();

                //navStruct.angularVelocityOfAsteroid = asteroidAngularVelocity;
                
                navStruct.angularVelocityOfAsteroid_Estimate = getFinalEstimatedAngularVelocity();
                std::cout << glm::to_string(navStruct.angularVelocityOfAsteroid_Estimate) << " estimated angular velocity \n";
                navStruct.estimationComplete = true;

                if(Service::OUTPUT_TEXT){
                    //output final estimation data to file
                    std::string time = std::to_string(p_mediator->physics_getTimeStamp());
                    p_mediator->writer_writeToFile("EST", "FINAL ESTIMATION");
                    std::string text = time + ":" + glm::to_string(navStruct.angularVelocityOfAsteroid_Estimate);
                    p_mediator->writer_writeToFile("EST", text);
                }
                estimateComplete = true;
            }
            else if(estimateComplete == false){
                //navStruct.angularVelocityOfAsteroid = asteroidAngularVelocity;
                estimateComplete = true;
                navStruct.estimationComplete = true;
            }
        }
        //else if(estimateComplete == false){
        //    navStruct.angularVelocityOfAsteroid = asteroidAngularVelocity;
       //     estimateComplete = true;
        //    navStruct.estimationComplete = true;
        //}
        
        //std::cout << glm::to_string(asteroidAngularVelocity) << " actual angular velocity \n";
        navStruct.approachDistance = approachDistance;
        navStruct.gravityVector = Service::bt2glm(p_lander->landerGravityVector);
        navStruct.landerTransformMatrix = p_lander->transformMatrix;
        navStruct.velocityVector = p_lander->landerVelocityVector;

        glm::vec3 calculatedThrustVector = gnc.getThrustVector(GNC_TIMER_SECONDS);
        if(glm::length(calculatedThrustVector) != 0);
            addImpulseToLanderQueue(1.0f, calculatedThrustVector.x, calculatedThrustVector.y, calculatedThrustVector.z, false);
    }

    //check if we have a boost command queued
    //this and corresponding queue need moved to Lander::CPU
    if(landerImpulseRequested()){
        LanderBoostCommand& nextBoost = popLanderImpulseQueue();
        if(nextBoost.torque) //wont be using torque for this because we can use reaction wheels if we have time
            applyTorque(body, nextBoost);
        else{
            //pass to ui to draw booster firing
            p_mediator->ui_submitBoostCommand(nextBoost);
            applyImpulse(body, nextBoost);
        }
            
    }
}

void CPU::showEstimationStats(){
    std::vector<glm::vec3> estimatedAngularVelocities = cv.getEstimatedAngularVelocities();
    for(glm::vec3 v : estimatedAngularVelocities){
        std::cout << glm::to_string(v) << " estimations \n";
    }
}

glm::vec3 CPU::getFinalEstimatedAngularVelocity(){
    
    std::vector<glm::vec3> estimatedAngularVelocities = cv.getEstimatedAngularVelocities();

    glm::vec3 posAxisCounter = glm::vec3(0,0,0);
    glm::vec3 negAxisCounter = glm::vec3(0,0,0);
    glm::vec3 posHighestAxisSum = glm::vec3(0,0,0);
    glm::vec3 negHighestAxisSum = glm::vec3(0,0,0);

    for(glm::vec3 v : estimatedAngularVelocities){
        int highestAxis = Service::getHighestAxis(v);
        if(v[highestAxis] > 0){
            posAxisCounter[highestAxis] += 1;
            posHighestAxisSum[highestAxis] += v[highestAxis];
        }
        else{
            negAxisCounter[highestAxis] += 1;
            negHighestAxisSum[highestAxis] += v[highestAxis];
        }
        
    }

    std::cout << glm::to_string(posAxisCounter) << " pos axis counters\n";
    std::cout << glm::to_string(posHighestAxisSum) << " pos highest axis sum\n";

    std::cout << glm::to_string(negAxisCounter) << " neg axis counters\n";
    std::cout << glm::to_string(negHighestAxisSum) << " neg highest axis sum\n";

    posHighestAxisSum[0] = posHighestAxisSum[0]/posAxisCounter[0];
    posHighestAxisSum[1] = posHighestAxisSum[1]/posAxisCounter[1];
    posHighestAxisSum[2] = posHighestAxisSum[2]/posAxisCounter[2];

    negHighestAxisSum[0] = negHighestAxisSum[0]/negAxisCounter[0];
    negHighestAxisSum[1] = negHighestAxisSum[1]/negAxisCounter[1];
    negHighestAxisSum[2] = negHighestAxisSum[2]/negAxisCounter[2];

    int posHighestAxisOccurance = Service::getHighestAxis(posAxisCounter);
    int negHighestAxisOccurance = Service::getHighestAxis(negAxisCounter);

    glm::vec3 posFinalEstimatedAngularVelocity = glm::vec3(0);
    posFinalEstimatedAngularVelocity[posHighestAxisOccurance] = posHighestAxisSum[posHighestAxisOccurance];

    glm::vec3 negFinalEstimatedAngularVelocity = glm::vec3(0);
    negFinalEstimatedAngularVelocity[negHighestAxisOccurance] = negHighestAxisSum[negHighestAxisOccurance];

    std::cout << glm::to_string(posFinalEstimatedAngularVelocity) << " pos axis final\n";
    std::cout << glm::to_string(negFinalEstimatedAngularVelocity) << " neg axis final\n";

    glm::vec3 bestEstimate;
    if(posAxisCounter[posHighestAxisOccurance] > negAxisCounter[negHighestAxisOccurance]){
        bestEstimate = posFinalEstimatedAngularVelocity;
    }
    else{
        bestEstimate = negFinalEstimatedAngularVelocity;
    }

    return bestEstimate;
}

void CPU::addImpulseToLanderQueue(float duration, float x, float y, float z, bool torque){
    landerBoostQueueLock.lock();
    landerBoostQueue.push_back(LanderBoostCommand{duration, glm::vec3{x,y,z}, torque});
    landerBoostQueueLock.unlock();
}

bool CPU::landerImpulseRequested(){
    landerBoostQueueLock.lock();
    bool empty = landerBoostQueue.empty();
    landerBoostQueueLock.unlock();
    return !empty;
}

LanderBoostCommand& CPU::popLanderImpulseQueue(){
    landerBoostQueueLock.lock();
    LanderBoostCommand& command = landerBoostQueue.front();
    landerBoostQueue.pop_front();
    landerBoostQueueLock.unlock();
    return command;
}

//this should be renamed, its more than an image timer
bool CPU::gncTimer(float timeStep){
    if(gncActive){
        gncTime += timeStep;
        if(gncTime > GNC_TIMER_SECONDS){
            //std::cout << "GNC Requested\n";
            gncTime = 0;
            return true;
        }
    }
    return false;
}

bool CPU::imagingTimer(float timeStep){
    if(imagingActive){
        imagingTime += timeStep;
        if(imagingTime > IMAGING_TIMER_SECONDS){
            std::cout << "Image Requested\n";
            imagingTime = 0;
            return true;
        }
    }
    return false;
}

void CPU::setRotation(){
    p_lander->rot = p_landingSite->rot;//we are cheating by setting the lander to rotation matrix, to the landing sites rot matrix
    //then we construct the transform matrix from this, and set for both renderer and bullet
    glm::mat4 scale = glm::scale(glm::mat4{ 1.0 }, p_lander->scale);
    glm::mat4 translation = glm::translate(glm::mat4{ 1.0 }, p_lander->pos);
    glm::mat4 rotation = p_lander->rot;
    p_lander->transformMatrix = translation * rotation * scale; //renderer transform
    p_lander->landerTransform.setBasis(Service::glmToBullet(p_lander->rot)); //bullet transform

    //ISSUE
    //While this means the contact is always perfect at touchdown, it means there is always unnatural rotation before that point
    //cant just set this right before landing either because the model would snap to a different rotation, would look even worse
    //
    //SOLUTION
    //check 2 or 3 times on approach, and align to expected tgo up vector
    //by calculating offset between current up vector and desired up vector and submitting a correction. 
    //if we are using Reaction Wheels, we dont need to submit torque commands, just slerp the rotation every tick
    //lerp or slerp towards desired rotation at touchdown time, maybe a minute before touchdown
    //still not sure of the maths though or even how to approach this one
}

//not implemented yet, need to work on this
/*void CPU::slewToLandingSiteOrientation(){
    glm::vec3 projectedLSUp = p_lander->cpu.gnc.getProjectedUpVector();

    //glm::mat4 projectedLSRotation = p_lander->cpu.gnc.getProjectedLSRotationMatrix();


    //glm::quat rotation = glm::quat_cast(rotMatrix);

    glm::quat predictedRotation =  glm::quat_cast(p_lander->cpu.gnc.getProjectedLSRotationMatrix());

    std::cout << glm::to_string(predictedRotation) << "\n";

    glm::quat currentRotation = glm::lookAt(glm::vec3(0), p_lander->up, p_lander->forward);

    glm::quat nextRotation = rotateTowards(currentRotation, predictedRotation, 0.01f);

    glm::mat4 nextRotationMatrix = glm::toMat4(nextRotation);

    p_lander->rot = nextRotationMatrix;//we are cheating by setting the lander to rotation matrix, to the landing sites rot matrix
    //then we construct the transform matrix from this, and set for both renderer and bullet
    glm::mat4 scale = glm::scale(glm::mat4{ 1.0 }, p_lander->scale);
    glm::mat4 translation = glm::translate(glm::mat4{ 1.0 }, p_lander->pos);
    glm::mat4 rotation = p_lander->rot;
    p_lander->transformMatrix = translation * rotation * scale; //renderer transform
    p_lander->landerTransform.setBasis(Service::glmToBullet(p_lander->rot)); //bullet transform

}*/

//http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-17-quaternions/#how-do-i-use-lookat-but-limit-the-rotation-at-a-certain-speed-
//example here, a slerp catching common issues
/*glm::quat CPU::rotateTowards(glm::quat q1, glm::quat q2, float maxAngle){
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

//these functions could be combined
void CPU::applyImpulse(btRigidBody* rigidbody, LanderBoostCommand boost){       
    btVector3 boostVector = Service::glm2bt(boost.vector);
    //std::cout << boostVector.length() << " boost vec\n";
    float force = BOOST_STRENGTH * boostVector.length();
    force = glm::clamp(force, 0.0f, LANDER_BOOST_CAP); //clamp boost force to a max value

    if(force > 0.0f){
        btVector3 boostDirection = boostVector.normalize();
        btMatrix3x3& rot = rigidbody->getWorldTransform().getBasis();
        btVector3 correctedForce = rot * boostDirection;
        //rigidbody->applyCentralForce(correctedForce*force);
        rigidbody->applyCentralImpulse(correctedForce*force);
    }
    else{
        std::cout << "This is strange, applyImpulse is being triggered when pausing/unpause or changing time\n";
    }
}

//dont need to use this, we will simulate reaction wheel behaviour instead
void CPU::applyTorque(btRigidBody* rigidbody, LanderBoostCommand boost){
    btVector3 relativeForce = Service::glm2bt(boost.vector);
    btMatrix3x3& rot = rigidbody->getWorldTransform().getBasis();
    btVector3 correctedForce = rot * relativeForce;
    rigidbody->applyTorqueImpulse(correctedForce);
}

void CPU::setAutopilot(bool b){
    reactionWheelEnabled = b;
    gncActive = b;//maybe need to move later
}

void CPU::setImaging(bool b){
    imagingActive = b;
}