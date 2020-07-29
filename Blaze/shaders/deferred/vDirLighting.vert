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

layout(set = 0, binding = 0) uniform CameraUBO {
	mat4 view;
	mat4 projection;
	vec3 viewPos;
	float farPlane;
} camera;

struct PointLightData {
	vec3 position;
	float brightness;
	float radius;
	int shadowIndex;
};

layout(push_constant) uniform LightIdx {
	int idx;
	int pad0_;
	int pad1_;
	int pad2_;
} pcb;

layout(set = 1, binding = 0) readonly buffer Lights {
	PointLightData data[];
} lights;

void main() {
	vec4 pos = camera.projection * camera.view * vec4(A_POSITION + camera.viewPos, 1.0);
	gl_Position = pos.xyww;
	O_POSITION = vec4(A_POSITION, 1.0f);
	O_POSITION.x *= -1.0f;
}
