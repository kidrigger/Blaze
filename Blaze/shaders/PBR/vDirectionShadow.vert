#version 450
#extension GL_EXT_multiview : enable

#define MAX_CASCADES 4

layout(location = 0) in vec3 A_POSITION;
layout(location = 1) in vec3 A_NORMAL;
layout(location = 2) in vec2 A_UV0;
layout(location = 3) in vec2 A_UV1;

layout(location = 0) out vec4 O_POSITION;

layout(set = 0, binding = 0) uniform ProjView {
	mat4 projection;
	mat4 view[MAX_CASCADES];
} views;

layout(push_constant) uniform PushConsts {
	mat4 model;
	vec3 lightDir;
} pcb;

void main() {
	gl_Position = views.projection * views.view[gl_ViewIndex] * O_POSITION;
}
