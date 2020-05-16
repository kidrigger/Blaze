#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(location = 0) in vec3 A_POSITION;
layout(location = 1) in vec3 A_NORMAL;
layout(location = 2) in vec2 A_UV0;
layout(location = 3) in vec2 A_UV1;

layout(location = 0) out vec3 outPosition;

layout(push_constant) uniform PushConsts {
	layout (offset = 0) mat4 mvp;
} trs;

void main() {
	outPosition = A_POSITION;
	outPosition.x *= -1.0f;
	gl_Position = trs.mvp * vec4(A_POSITION.xyz, 1.0f);
}
