#version 450
#extension GL_EXT_multiview : enable

layout(location = 0) in vec3 A_POSITION;
layout(location = 1) in vec3 A_NORMAL;
layout(location = 2) in vec2 A_UV0;
layout(location = 3) in vec2 A_UV1;

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outLightPos;

layout(set = 0, binding = 0) uniform ProjView {
	mat4 view[6];
} pv;

layout(push_constant) uniform PushConsts {
	layout (offset = 0) mat4 mvp;
	layout (offset = 64) mat4 proj;
	layout (offset = 128) vec3 lightPos;
} trs;

void main() {
	outPosition = (trs.mvp * vec4(A_POSITION, 1.0f)).xyz;
	gl_Position = trs.proj * pv.view[gl_ViewIndex] * (trs.mvp * vec4(A_POSITION.xyz, 1.0f) - vec4(trs.lightPos, 0.0f));
	outLightPos = trs.lightPos;
}
