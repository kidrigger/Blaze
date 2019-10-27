#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoords0;
layout(location = 3) in vec2 inTexCoords1;

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec2 outTexCoord0;

void main() {
	outPosition = inPosition;
	outTexCoord0 = inTexCoords0;
	gl_Position = vec4(inPosition.xyz, 1.0f);
}
