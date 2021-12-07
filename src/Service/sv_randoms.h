#pragma once
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
class btVector3;
class btMatrix3x3;
class btTransform;
class btQuaternion;

namespace Service{
    btVector3 getPointOnSphere(float pitch, float yaw, float radius); //helper, gets point on sphere of given radius, origin 0
    float getRandFloat(float min, float max); //helper, gets random float between min and max

    btVector3 glm2bt(const glm::vec3& vec);

    glm::vec3 bt2glm(const btVector3& vec);

    btMatrix3x3 glmToBullet(const glm::mat3& m);

    glm::mat4 bulletToGlm(const btTransform &t);

    btTransform glmToBulletT(const glm::mat4 &m);

    glm::quat bulletToGlm(const btQuaternion& q);

    btQuaternion glmToBulletQ(const glm::quat &q);

}