#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_multiview : enable

layout(location = 0) in vec3 A_POSITION;
layout(location = 1) in vec3 A_NORMAL;
layout(location = 2) in vec2 A_UV0;
layout(location = 3) in vec2 A_UV1;

layout(location = 0) out vec3 outPosition;

layout(set = 0, binding = 0) uniform ProjView {
	mat4 projection;
	mat4 view[6];
} pv;

layout(push_constant) uniform PushConsts {
	mat4 mvp;
	float deltaPhi;
	float deltaTheta;
} trs;

void main() {
	outPosition = A_POSITION;
	outPosition.x *= -1.0f;
	gl_Position = pv.projection * pv.view[gl_ViewIndex] * trs.mvp * vec4(A_POSITION.xyz, 1.0f);
}
