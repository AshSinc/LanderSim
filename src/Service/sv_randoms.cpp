#include "sv_randoms.h"
#include <bullet/btBulletDynamicsCommon.h>
#include <LinearMath/btMatrix3x3.h>
#include <LinearMath/btVector3.h>
#include <random>
#include <iomanip> //used for setprecision in random function

#define GLM_FORCE_RADIANS                  //makes sure GLM uses radians to avoid confusion
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES //forces GLM to use a version of vec2 and mat4 that have the correct alignment requirements for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE        //forces GLM to use depth range of 0 to 1, instead of -1 to 1 as in OpenGL

#include <iostream>

//helper, gets point on sphere of given radius, origin 0
btVector3 Service::getPointOnSphere(float pitch, float yaw, float radius)
{
    btVector3 result;
    std::cout << pitch << "\n";
    result.setX(radius * cos(glm::radians(yaw)) * sin(glm::radians(pitch)));
    result.setY(radius * sin(glm::radians(yaw)) * sin(glm::radians(pitch)));
    result.setZ(radius * cos(glm::radians(pitch)));

    return result;
}

//helper, gets random float between min and max
float Service::getRandFloat(float min, float max)
{
    float offset = 0.0f;
    if (min < 0)
    {
        offset = -min;
        min = 0;
        max += offset;
    }
    std::random_device rd;
    std::default_random_engine eng(rd());
    std::uniform_real_distribution<> distr(min, max);
    //std::setprecision(10);
    float result = distr(eng) - offset;
    //std::cout << result << "\n";
    return result;
}

btVector3 Service::glm2bt(const glm::vec3 &vec)
{
    return {vec.x, vec.y, vec.z};
}

glm::vec3 Service::bt2glm(const btVector3 &vec)
{
    return {vec.getX(), vec.getY(), vec.getZ()};
}

btMatrix3x3 Service::glmToBullet(const glm::mat3 &m)
{
    return btMatrix3x3(m[0][0], m[1][0], m[2][0], m[0][1], m[1][1], m[2][1], m[0][2], m[1][2], m[2][2]);
}

// https://pybullet.org/Bullet/phpBB3/viewtopic.php?t=11462
//glm::vec3 bulletToGlm(const btVector3& v) { return glm::vec3(v.getX(), v.getY(), v.getZ()); }

//btVector3 glmToBullet(const glm::vec3& v) { return btVector3(v.x, v.y, v.z); }

glm::quat Service::bulletToGlm(const btQuaternion& q){ 
    return glm::quat(q.getW(), q.getX(), q.getY(), q.getZ()); 
}

btQuaternion Service::glmToBulletQ(const glm::quat &q) { return btQuaternion(q.x, q.y, q.z, q.w); }

//btMatrix3x3 glmToBullet(const glm::mat3& m) { return btMatrix3x3(m[0][0], m[1][0], m[2][0], m[0][1], m[1][1], m[2][1], m[0][2], m[1][2], m[2][2]); }

// btTransform does not contain a full 4x4 matrix, so this transform is lossy.
// Affine transformations are OK but perspective transformations are not.
btTransform Service::glmToBulletT(const glm::mat4 &m)
{
    glm::mat3 m3(m);
    return btTransform(glmToBullet(m3), glm2bt(glm::vec3(m[3][0], m[3][1], m[3][2])));
}

glm::mat4 Service::bulletToGlm(const btTransform &t)
{
    glm::mat4 m = glm::mat4();
    const btMatrix3x3 &basis = t.getBasis();
    // rotation
    for (int r = 0; r < 3; r++)
    {
        for (int c = 0; c < 3; c++)
        {
            m[c][r] = basis[r][c];
        }
    }
    // traslation
    btVector3 origin = t.getOrigin();
    m[3][0] = origin.getX();
    m[3][1] = origin.getY();
    m[3][2] = origin.getZ();
    // unit scale
    m[0][3] = 0.0f;
    m[1][3] = 0.0f;
    m[2][3] = 0.0f;
    m[3][3] = 1.0f;
    return m;
}

glm::mat4 Service::openCVToGlm(const cv::Mat &m){
    glm::mat4 returnMat;
    returnMat[0][0] = m.at<_Float64>(0,0);
    returnMat[0][1] = m.at<_Float64>(0,1);
    returnMat[0][2] = m.at<_Float64>(0,2);
    returnMat[0][3] = m.at<_Float64>(0,3);
    returnMat[1][0] = m.at<_Float64>(1,0);
    returnMat[1][1] = m.at<_Float64>(1,1);
    returnMat[1][2] = m.at<_Float64>(1,2);
    returnMat[1][3] = m.at<_Float64>(1,3);
    returnMat[2][0] = m.at<_Float64>(2,0);
    returnMat[2][1] = m.at<_Float64>(2,1);
    returnMat[2][2] = m.at<_Float64>(2,2);
    returnMat[2][3] = m.at<_Float64>(2,3);
    returnMat[3][0] = m.at<_Float64>(3,0);
    returnMat[3][1] = m.at<_Float64>(3,1);
    returnMat[3][2] = m.at<_Float64>(3,2);
    returnMat[3][3] = m.at<_Float64>(3,3);
    return returnMat;
}

int Service::getHighestAxis(glm::vec3 v){
    int highestAxis = 0;
    float highestValue = abs(v.x);
    if (highestValue < abs(v.y)){
        highestValue = abs(v.y);
        highestAxis = 1;
    }
    if (highestValue < abs(v.z)){
        highestValue = abs(v.z);
        highestAxis = 2;
    }
    return highestAxis;
}