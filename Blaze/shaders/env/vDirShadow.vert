#version 450

layout(location = 0) in vec3 A_POSITION;
layout(location = 1) in vec3 A_NORMAL;
layout(location = 2) in vec2 A_UV0;
layout(location = 3) in vec2 A_UV1;

layout(location = 0) out vec4 outPosition;

layout(set = 0, binding = 0) uniform ProjView {
	mat4 view[6];
} pv;

layout(push_constant) uniform PushConsts {
	layout (offset = 0) mat4 model;
	layout (offset = 64) mat4 pv;
	layout (offset = 128) vec3 lightDir;
} trs;

void main() {
	outPosition = trs.model * vec4(A_POSITION.xyz, 1.0f);
	gl_Position = trs.pv * trs.model * vec4(A_POSITION.xyz, 1.0f);
}
