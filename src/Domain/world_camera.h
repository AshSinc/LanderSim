
#define GLM_FORCE_RADIANS //makes sure GLM uses radians to avoid confusion
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES //forces GLM to use a version of vec2 and mat4 that have the correct alignment requirements for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE //forces GLM to use depth range of 0 to 1, instead of -1 to 1 as in OpenGL
#include <glm/glm.hpp>
#include <vector>

class WorldObject;
class Mediator;

struct CameraData{
    glm::vec3 cameraPos = glm::vec3(2.0f, 2.0f, 2.0f);
    glm::vec3 cameraFront = glm::vec3(-2.0f, -2.0f, -2.0f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 0.0f, 1.0f);
};

class WorldCamera{
private:
    bool mouselookActive = true;
    Mediator& r_mediator;
    bool freelook = false;
    const float CAMERA_SPEED = 5;
    float fixedLookRadius = 2;  
    float fixedObjectScaleFactor = 1;        
    void updateCamera(glm::vec3 newPos, glm::vec3 front);

    int objectFocusIndex = 2; //this needs changed to point to an actual object now, objects in scene need to be gettable, maybe put back in an unnordered map
    CameraData cameraData;

    float lastMouseX, lastMouseY, yaw, pitch;
    bool firstMouseInput = true;     
    const float MOUSELOOK_SENSITIVITY = 0.1f;
    const float MOUSESCROLL_SENSITIVITY = 0.5f;

    bool usingAutoCamera = false;
    int autoCameraObjectFocusIndex = 1; 

public:   
    void calculatePitchYaw(double xpos, double ypos);
    WorldCamera(Mediator& mediator);
    CameraData* getCameraDataPtr();
    void updateFixedLookPosition();
    void changeFocus();
    void calculateZoom(float yoffset);
    void setMouseLookActive(bool state);
};