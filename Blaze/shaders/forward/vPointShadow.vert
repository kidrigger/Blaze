#version 450
#extension GL_EXT_multiview : enable

layout(location = 0) in vec3 A_POSITION;
layout(location = 1) in vec3 A_NORMAL;
layout(location = 2) in vec2 A_UV0;
layout(location = 3) in vec2 A_UV1;

layout(location = 0) out vec4 O_POSITION;

layout(set = 0, binding = 0) uniform ProjView {
	mat4 projection;
	mat4 view[6];
} views;

layout(push_constant) uniform PushConsts {
	mat4 model;
	vec3 lightPos;
	float radius;
	float p22;
	float p32;
} pcb;

void main() {
	mat4 proj = views.projection;
	proj[2][2] = pcb.p22;
	proj[3][2] = pcb.p32;
	O_POSITION = pcb.model * vec4(A_POSITION, 1.0f);
	gl_Position = proj * views.view[gl_ViewIndex] * (O_POSITION - vec4(pcb.lightPos, 0.0f));
}
