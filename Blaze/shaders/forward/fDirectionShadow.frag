#version 450

layout(push_constant) uniform PushConsts {
	mat4 model;
	mat4 PV;
} pcb;

void main() {
	gl_FragDepth = gl_FragCoord.z;
}
