#version 460 //using 460 instead of 450 gives us gl_BaseInstance, which is an index into a storage buffer
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition; 
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord; //pass through texCoord to fragment shader
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragPos;

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
	//gl_BaseInstance is the firstInstance integer parameter of draw calls, simple way to pass an integer to shader, even if not using isntanced rendering
	ObjectData object = objectBuffer.objects[gl_BaseInstance];
    mat4 transformMatrix = (cameraData.viewproj * object.modelmtx);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
	fragNormal = mat3(object.normalmtx) * inNormal; //get the frag normals into view space by using the normal matrix
	fragPos = vec3(cameraData.view * object.modelmtx * vec4(inPosition, 1.0f));
	gl_Position = transformMatrix * vec4(inPosition, 1.0f);
}