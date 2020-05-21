#version 450
#extension GL_EXT_multiview : enable

layout(location = 0) in vec3 A_POSITION;
layout(location = 1) in vec3 A_NORMAL;
layout(location = 2) in vec2 A_UV0;
layout(location = 3) in vec2 A_UV1;

layout(set = 0, binding = 0) uniform ProjView {
	mat4 projection;
	mat4 view[6];
} views;

layout(push_constant) uniform PushConsts {
	mat4 model;
	vec3 lightPos;
	float radius;
} pcb;

void main() {
	gl_Position = views.projection * views.view[gl_ViewIndex] * (vec4(vec3(1.0f/pcb.radius), 1.0f) * (pcb.model * vec4(A_POSITION, 1.0f) - vec4(pcb.lightPos, 0.0f)));
}
