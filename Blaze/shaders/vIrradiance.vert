#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoords0;
layout(location = 3) in vec2 inTexCoords1;

layout(location = 0) out vec3 outPosition;

layout(push_constant) uniform PushConsts {
	layout (offset = 0) mat4 mvp;
} trs;

void main() {
	outPosition = inPosition;
	outPosition.x *= -1.0f;
	gl_Position = trs.mvp * vec4(inPosition.xyz, 1.0f);
}
