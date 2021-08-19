#version 450

layout(set = 0, binding = 1) uniform SceneData{   
	vec4 lightDirection;
    vec4 lightAmbient;
    vec4 lightDiffuse;
    vec4 lightSpecular;
} sceneData;

layout (set = 2, binding = 0) uniform samplerCube samplerCubeMap;

layout (location = 0) in vec3 fragTexCoord;
layout (location = 1) in vec3 fragPos;

layout (location = 0) out vec4 outColor;

const float intensityConstant = 0.8f; //about 50 degrees

void main(){
	////use the suns light direction and -fragPos to calulate intensity of light in a radius, 
	//similar to spotlight, but we use this to block out the star map with an approximation
	vec3 lightDir = normalize(vec3(sceneData.lightDirection));
	vec3 viewDir = normalize(-fragPos);
	float similarity = dot(viewDir,lightDir);

	similarity = intensityConstant - similarity;

	outColor = texture(samplerCubeMap, fragTexCoord) * similarity;
}