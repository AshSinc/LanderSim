
#define GLM_FORCE_RADIANS //makes sure GLM uses radians to avoid confusion
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES //forces GLM to use a version of vec2 and mat4 that have the correct alignment requirements for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE //forces GLM to use depth range of 0 to 1, instead of -1 to 1 as in OpenGL
#include <glm/glm.hpp>
#include <vector>
#include <string>

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
    float fixedLookRadius = 10;
    const float MAX_FIXED_LOOK_RADIUS = 20;
    float fixedObjectScaleFactor = 1;        
    void updateCamera(glm::vec3 newPos, glm::vec3 front);

    //const std::string FOCUS_NAMES[4] {"Lander", "Asteroid", "Landing_Site", "Debug_Box"};
    const std::string FOCUS_NAMES[3] {"Lander", "Asteroid", "Landing_Site"};
    const int FOCUS_NAMES_SIZE = sizeof(FOCUS_NAMES)/sizeof(FOCUS_NAMES[0]);
    int objectFocusIndex = 0;
    CameraData cameraData;

    float lastMouseX, lastMouseY, yaw, pitch;
    bool firstMouseInput = true;     
    const float MOUSELOOK_SENSITIVITY = 0.1f;
    const float MOUSESCROLL_SENSITIVITY = 0.5f;

    bool usingAutoCamera = true;

    void updateAutoCamera(float cameraDistanceScale);
    float xOffset, yOffset;
public:   
    void calculatePitchYaw(double xpos, double ypos);
    WorldCamera(Mediator& mediator);
    CameraData* getCameraDataPtr();
    void updateFixedLookPosition();
    void changeFocus();
    void calculateZoom(float yoffset);
    void setMouseLookActive(bool state);
    void init();
    void toggleAutoCamera();
};