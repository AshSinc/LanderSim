#pragma once

class btVector3;

namespace Service{
    btVector3 getPointOnSphere(float pitch, float yaw, float radius); //helper, gets point on sphere of given radius, origin 0
    float getRandFloat(float min, float max); //helper, gets random float between min and max
}