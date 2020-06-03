#version 450
#extension GL_EXT_multiview : enable

#define MAX_CASCADES 4

layout(location = 0) in vec3 A_POSITION;
layout(location = 1) in vec3 A_NORMAL;
layout(location = 2) in vec2 A_UV0;
layout(location = 3) in vec2 A_UV1;

layout(push_constant) uniform PushConsts {
	mat4 model;
	mat4 PV;
} pcb;

void main() {
	gl_Position = pcb.PV * pcb.model * vec4(A_POSITION, 1.0f);
}
