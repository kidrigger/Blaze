#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inLightPos;

layout(location = 0) out vec4 outColor;

void main() {
	outColor = vec4(vec3(length(inPosition - inLightPos)), 1.0f);
}
