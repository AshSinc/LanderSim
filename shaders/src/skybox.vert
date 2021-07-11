#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor; //not used
layout(location = 2) in vec2 inTexCoord; //not used
layout(location = 3) in vec3 inNormal; //not used

layout (location = 0) out vec3 fragTexCoord;
layout (location = 1) out vec3 fragPos;

layout( set = 0, binding = 0 ) uniform  CameraBuffer{   
	mat4 view;
	mat4 proj;
	mat4 viewproj;
} cameraData;

//an individual object consists of
struct ObjectData{
	mat4 modelmtx;
	mat4 normalmtx;
}; 

//all object matrices
//std140 enforces rules about memory layout and alignment
//set is 1 and binding is 0 because its a new descriptor set slot
layout( std140, set = 1, binding = 0 ) readonly buffer ObjectBuffer{   
	ObjectData objects[];
} objectBuffer;

void main(){
    //we are using this to get fragPos to pass to frag shader, only to calc if we are looking towards the scene light and should block some of the texture
	ObjectData object = objectBuffer.objects[gl_BaseInstance];

    fragTexCoord = inPosition;
    fragTexCoord.y *= -1.0; //flip the y coordinate to get it into vulkan coordinates
    mat4 staticView = cameraData.view;
    staticView[3] = vec4(0.0f, 0.0f, 0.0f, 1.0f); //remove the translation element of the matrix so we dont move it with the camera
    fragPos = vec3(staticView * object.modelmtx * vec4(inPosition, 1.0f));
    vec4 pos = cameraData.proj * staticView * vec4(inPosition, 1.0);
    gl_Position = pos.xyww; //OpenGL book optimization
    //in draw call draw skybox last but set depth stencil to less_or_equal and then replace the z depth coordinate here with w
    //then hlsl will use z/w to get depth, which will always be 1, drawing at the back but discarding blocked fragments
}