#pragma once
#include <glm/glm.hpp>
class btVector3;

namespace Service{
    btVector3 getPointOnSphere(float pitch, float yaw, float radius); //helper, gets point on sphere of given radius, origin 0
    float getRandFloat(float min, float max); //helper, gets random float between min and max

    btVector3 glm2bt(const glm::vec3& vec);

    glm::vec3 bt2glm(const btVector3& vec);
}