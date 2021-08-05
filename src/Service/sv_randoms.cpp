#include "sv_randoms.h"

#include <LinearMath/btVector3.h>
#include <random>
#include <iomanip> //used for setprecision in random function

#define GLM_FORCE_RADIANS //makes sure GLM uses radians to avoid confusion
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES //forces GLM to use a version of vec2 and mat4 that have the correct alignment requirements for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE //forces GLM to use depth range of 0 to 1, instead of -1 to 1 as in OpenGL
#include <glm/glm.hpp>

//helper, gets point on sphere of given radius, origin 0
btVector3 Service::getPointOnSphere(float pitch, float yaw, float radius){
    btVector3 result;
    result.setX(radius * cos(glm::radians(yaw)) * sin(glm::radians(pitch)));
    result.setY(radius * sin(glm::radians(yaw)) * sin(glm::radians(pitch)));
    result.setZ(radius * cos(glm::radians(pitch)));
    return result;
}

//helper, gets random float between min and max
float Service::getRandFloat(float min, float max){
    float offset = 0.0f;
    if(min < 0){
        offset = -min;
        min = 0;
        max += offset;
    }
    std::random_device rd;
    std::default_random_engine eng(rd());
    std::uniform_real_distribution<> distr(min, max);
    std::setprecision(10);
    float result = distr(eng)-offset;
    //std::cout << result << "\n";
    return result;
}
