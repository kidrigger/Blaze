#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoords0;
layout(location = 3) in vec2 inTexCoords1;

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
	outPosition = trs.model * vec4(inPosition.xyz, 1.0f);
	gl_Position = trs.pv * trs.model * vec4(inPosition.xyz, 1.0f);
}
