#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 1) uniform SceneData{   
	vec4 lightDirection;
    vec4 lightAmbient;
    vec4 lightDiffuse;
    vec4 lightSpecular;
} sceneData;

//push constants block
layout( push_constant ) uniform constants{
	int matIndex;
	int numPointLights;
	int numSpotLights;
} PushConstants;

//material object to determine its material properties (how much light it reflects)
struct MaterialData{
	vec3 diffuse;
    vec3 specular;
    vec3 extra; //x is shininess, y and z unused
}; 

//all object matrices
//std140 enforces rules about memory layout and alignment
//set is 2 and binding is 0 because its a new descriptor set slot
layout( std140, set = 2, binding = 0 ) uniform MaterialBuffer{   
	MaterialData materials[4]; //WARNING!!! statically set materials max size in shader :(  could use storage buffer instead? there must be a way with uniform buffer though?
} materialBuffer;

//point lighting object holds point light parameters
struct PointLightingData{
	vec3 pos;
	vec3 ambient;
	vec3 diffuse;
    vec3 specular;
	vec3 attenuation;
}; 

//point lighting object holds point light parameters
struct SpotLightingData{
	vec3 pos;
	vec3 ambient;
	vec3 diffuse;
    vec3 specular;
	vec3 attenuation;
	vec3 direction; //z is angular cutoff
	vec2 cutoffs; //x is inner cutoff y is outer cutoff
}; 

layout( std140, set = 3, binding = 0 ) uniform PointLightingBuffer{   
	PointLightingData lights[10]; //WARNING!!! statically set light max size in shader
} pointLightingBuffer;

layout( std140, set = 3, binding = 1 ) uniform SpotLightingBuffer{   
	SpotLightingData lights[10]; //WARNING!!! statically set light max size in shader
} spotLightingBuffer;

layout(set = 4, binding = 0) uniform sampler2D diffSampler;
layout(set = 4, binding = 1) uniform sampler2D specSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

vec3 CalcDirLight(vec3 normal, vec3 viewDir);
vec3 CalcPointLight(PointLightingData light, vec3 normal, vec3 fragPos, vec3 viewDir);
vec3 CalcSpotLight(SpotLightingData light, vec3 normal, vec3 fragPos, vec3 viewDir);

MaterialData material = materialBuffer.materials[PushConstants.matIndex];

void main() {

	vec3 norm = normalize(fragNormal); //we need the normal of the fragment
	vec3 viewDir = normalize(-fragPos);

	vec3 result = CalcDirLight(norm, viewDir);

	for(int i = 0; i < PushConstants.numPointLights; i++)
		result += CalcPointLight(pointLightingBuffer.lights[i], norm, fragPos, viewDir);

	for(int i = 0; i < PushConstants.numSpotLights; i++)
		result += CalcSpotLight(spotLightingBuffer.lights[i], norm, fragPos, viewDir);

	float gamma = 1.1;
	vec3 gammaCorrected = pow(result, vec3(1.0/gamma));
	//outColor = vec4(result, 1.0);
	outColor = vec4(gammaCorrected, 1.0);
}

vec3 CalcDirLight(vec3 normal, vec3 viewDir){

	vec3 lightDir = normalize(vec3(-sceneData.lightDirection));
	// diffuse shading
	float diff = max(dot(normal, lightDir), 0.0);
	// specular shading
	vec3 halfwayDir = normalize(lightDir + viewDir); //blinn-phong specular lighting
	float spec;
	spec = pow(max(dot(normal, halfwayDir), 0.0), material.extra.x);
	spec *= max(dot(lightDir, normal), 0.0); //to catch if the light is behind the model dot(lightDir, normal) will be < 0, so we clamp to 0 and multiply it
	// combine results
	vec3 ambient = vec3(sceneData.lightAmbient) * vec3(texture(diffSampler, fragTexCoord));
	vec3 diffuse = vec3(sceneData.lightDiffuse) * diff * vec3(texture(diffSampler, fragTexCoord));
	vec3 specular = vec3(sceneData.lightSpecular) * spec * vec3(texture(specSampler,	fragTexCoord));
	return (ambient + diffuse + specular);
}

vec3 CalcPointLight(PointLightingData light, vec3 normal, vec3 fragPos, vec3 viewDir){

	vec3 lightDir = normalize(light.pos - fragPos);
	// diffuse shading
	float diff = max(dot(normal, lightDir), 0.0);
	// specular shading
	vec3 halfwayDir = normalize(lightDir + viewDir); //blinn-phong specular lighting
	float spec;
	spec = pow(max(dot(normal, halfwayDir), 0.0), material.extra.x);
	spec *= max(dot(lightDir, normal), 0.0); //to catch if the light is behind the model dot(lightDir, normal) will be < 0, so we clamp to 0 and multiply it
	// attenuation
	float distance = length(light.pos - fragPos);
	float attenuation = 1.0 / (light.attenuation.x + light.attenuation.y * distance + light.attenuation.z * (distance * distance));
	// combine results
	vec3 ambient = light.ambient * vec3(texture(diffSampler, fragTexCoord));
	vec3 diffuse = light.diffuse * diff * vec3(texture(diffSampler, fragTexCoord));
	vec3 specular = light.specular * spec * vec3(texture(specSampler, fragTexCoord));
	ambient *= attenuation;
	diffuse *= attenuation;
	specular *= attenuation;
	return (ambient + diffuse + specular);
}

vec3 CalcSpotLight(SpotLightingData light, vec3 normal, vec3 fragPos, vec3 viewDir){
	//used variables
	vec3 lightDir = normalize(light.pos - fragPos);
	float theta	= dot(lightDir, normalize(-light.direction));
	float epsilon = light.cutoffs.x - light.cutoffs.y;
	float intensity = clamp((theta - light.cutoffs.y) / epsilon, 0.0, 1.0);

	// diffuse shading
	float diff = max(dot(normal, lightDir), 0.0);
	// specular shading
	vec3 halfwayDir = normalize(lightDir + viewDir); //blinn-phong specular lighting
	float spec;
	spec = pow(max(dot(normal, halfwayDir), 0.0), material.extra.x);
	spec *= max(dot(lightDir, normal), 0.0); //to catch if the light is behind the model dot(lightDir, normal) will be < 0, so we clamp to 0 and multiply it
	// attenuation
	float distance = length(light.pos - fragPos);
	float attenuation = 1.0 / (light.attenuation.x + light.attenuation.y * distance + light.attenuation.z * (distance * distance));
	// combine results
	//vec3 ambient = light.ambient * material.diffuse;
	vec3 diffuse = light.diffuse * diff * vec3(texture(diffSampler, fragTexCoord));
	vec3 specular = light.specular * spec * vec3(texture(specSampler, fragTexCoord));
	//ambient *= attenuation;
	diffuse *= attenuation * intensity;
	specular *= attenuation * intensity;
	return diffuse + specular;
}