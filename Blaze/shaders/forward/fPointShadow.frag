#version 450

layout(location = 0) in vec4 V_POSITION;

layout(push_constant) uniform PushConsts {
	mat4 model;
	vec3 lightPos;
	float radius;
	float p22;
	float p23;
} pcb;

void main() {
	float z_dist = length(V_POSITION.xyz - pcb.lightPos);
	gl_FragDepth = z_dist / pcb.radius;
}
