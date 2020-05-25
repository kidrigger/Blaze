#version 450

layout(location = 0) in vec4 V_POSITION;

layout(push_constant) uniform PushConsts {
	mat4 model;
	vec3 lightDir;
} pcb;

void main() {

}
