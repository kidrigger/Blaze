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
	vec2 screenSize;
} camera;

layout(push_constant) uniform ModelBlock {
	float only_for_compatibility_with_PBR_[32];
} pcb;

void main() {
	vec4 pos = camera.projection * camera.view * vec4(A_POSITION * 0.1f + camera.viewPos, 1.0);
	gl_Position = pos.xyww;
	O_POSITION = vec4(A_POSITION, 1.0f);
	O_POSITION.x *= -1.0f;
}
