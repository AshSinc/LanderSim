#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 1) uniform SceneData{   
	vec4 lightDirection;
    vec4 lightAmbient;
    vec4 lightDiffuse;
    vec4 lightSpecular;
} sceneData;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec2 fragNormal; //default_lit passes this, causes warning but no harm, should still make an unlit vert shader
layout(location = 3) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

void main() {
    float greyscale = dot(fragColor, vec3(0.299, 0.587, 0.114));

	outColor = vec4(greyscale, greyscale, greyscale, 1.0);
}