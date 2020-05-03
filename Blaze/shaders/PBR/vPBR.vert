#version 450

layout(location = 0) in vec3 A_POSITION;
layout(location = 1) in vec3 A_NORMAL;
layout(location = 2) in vec2 A_UV0;
layout(location = 3) in vec2 A_UV1;

layout(location = 0) out vec4 outPosition;

layout(set = 0, binding = 0) uniform CameraUBO {
	mat4 view;
	mat4 projection;
	vec3 viewPos;
	float farPlane;
} camera;

layout(push_constant) uniform ModelBlock {
	mat4 model;
} pcb;

void main() {
	gl_Position = camera.projection * camera.view * pcb.model * vec4(A_POSITION, 1.0f);
	outPosition = pcb.model * vec4(A_POSITION, 1.0f);
}
