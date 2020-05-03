#version 450

layout(location = 0) in vec3 A_POSITION;
layout(location = 1) in vec3 A_NORMAL;
layout(location = 2) in vec2 A_UV0;
layout(location = 3) in vec2 A_UV1;

layout(location = 0) out vec4 O_POSITION;
layout(location = 1) out vec4 O_NORMAL;
layout(location = 2, component = 0) out vec2 O_UV0;
layout(location = 2, component = 2) out vec2 O_UV1;

layout(set = 0, binding = 0) uniform CameraUBO {
	mat4 view;
	mat4 projection;
	vec3 viewPos;
	float farPlane;
} camera;

layout(push_constant) uniform ModelBlock {
	mat4 model;
	float opaque_[16];
} pcb;

void main() {
	gl_Position = camera.projection * camera.view * pcb.model * vec4(A_POSITION, 1.0f);
	O_POSITION = pcb.model * vec4(A_POSITION, 1.0f);
	O_NORMAL = transpose(inverse(pcb.model)) * vec4(A_NORMAL, 0.0f);
	O_UV0 = A_UV0;
	O_UV1 = A_UV1;
}
