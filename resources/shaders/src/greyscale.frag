#version 450
#extension GL_ARB_separate_shader_objects : enable

//A combined image sampler descriptor is represented in GLSL by a sampler uniform.
layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

float grayScale = dot(texture(texSampler, fragTexCoord).rgb, vec3(0.299, 0.587, 0.114));

void main() {
    outColor = vec4(grayScale, grayScale, grayScale, 1.0);
}