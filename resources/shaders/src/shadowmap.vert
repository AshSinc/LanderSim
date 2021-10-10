#version 460
 #extension GL_ARB_separate_shader_objects : enable
//#extension GL_ARB_shading_language_420pack : enable

layout(std140, set = 0, binding = 0) uniform vp_ubo{
    mat4 viewproj;
} lightVpMatrix;
 
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
 
layout(location = 0) in vec3 inPosition; 
layout(location = 1) in vec3 inColor; //dont need these
layout(location = 2) in vec2 inTexCoord; //dont need these
layout(location = 3) in vec3 inNormal; //dont need these

layout(location = 0) out vec2 out_uv;
 
void main(){
    ObjectData object = objectBuffer.objects[gl_BaseInstance];
    vec4 pos = vec4(inPosition.x, inPosition.y, inPosition.z, 1.0);
    vec4 worldPos = object.modelmtx * pos;
    gl_Position = lightVpMatrix.viewproj * worldPos;
    out_uv = inTexCoord;
}