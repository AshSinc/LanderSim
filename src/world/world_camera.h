
#define GLM_FORCE_RADIANS //makes sure GLM uses radians to avoid confusion
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES //forces GLM to use a version of vec2 and mat4 that have the correct alignment requirements for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE //forces GLM to use depth range of 0 to 1, instead of -1 to 1 as in OpenGL
#include <glm/glm.hpp>
#include <vector>

class WorldObject;

struct CameraData{
    glm::vec3 cameraPos = glm::vec3(2.0f, 2.0f, 2.0f);
    glm::vec3 cameraFront = glm::vec3(-2.0f, -2.0f, -2.0f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 0.0f, 1.0f);
};

class WorldCamera{
private:
    bool freelook = false;
    const float CAMERA_SPEED = 5;
    float fixedLookRadius = 2;  
    float fixedObjectScaleFactor = 1;        
    void updateCamera(glm::vec3 newPos, glm::vec3 front);
    std::vector<WorldObject>& r_objects;
    int objectFocusIndex = 2; //start at the first worldState.object[i] that we want to be able to track, have to set in update function too
    CameraData cameraData;
public:   
    WorldCamera(std::vector<WorldObject>& r_objects);
    CameraData* getCameraDataPtr();
    void updateFixedLookPosition(float pitch, float yaw);
    void changeFocus();
    void changeZoom(float yoffset);
};