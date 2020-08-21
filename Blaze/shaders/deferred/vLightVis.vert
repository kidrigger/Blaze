#version 450

#define MAX_TEX_IN_MAT 32
#define MAX_POINT_LIGHTS 16
#define MAX_DIRECTION_LIGHTS 4
#define MAX_SHADOWS 16
#define MAX_CASCADES 4

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
	float ambientBrightness;
	vec2 screenSize;
	float nearPlane;
	float farPlane;
} camera;

layout(push_constant) uniform ModelBlock {
	mat4 model;
	vec4 color;
} pcb;

void main() {
	O_POSITION = pcb.model * vec4(A_POSITION, 1.0f);
	gl_Position = camera.projection * camera.view * O_POSITION;
	O_NORMAL = transpose(inverse(pcb.model)) * vec4(A_NORMAL, 0.0f);
	O_UV0 = A_UV0;
	O_UV1 = A_UV1;
}
