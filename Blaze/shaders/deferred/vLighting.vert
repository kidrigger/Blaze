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
layout(location = 1, component = 0) out vec2 O_UV0;

layout(set = 0, binding = 0) uniform CameraUBO {
	mat4 view;
	mat4 projection;
	vec3 viewPos;
	float ambientBrightness;
	vec2 screenSize;
	float nearPlane;
	float farPlane;
} camera;

layout(push_constant) uniform LightIdx {
	int idx;
	int pad0_;
	int pad1_;
	int pad2_;
} pcb;

struct PointLightData {
	vec3 position;
	float radius;
	vec3 color;
	int shadowIndex;
};

struct DirLightData {
	vec3 direction;
	float brightness;
	vec4 cascadeSplits;
	mat4 cascadeViewProj[MAX_CASCADES];
	int numCascades;
	int shadowIndex;
};

layout(set = 2, binding = 0) readonly buffer Lights {
	PointLightData data[];
} lights;

layout(set = 2, binding = 1) readonly buffer DirLights {
	DirLightData data[];
} dirLights;

void main() {
	O_POSITION = vec4(lights.data[pcb.idx].position + (A_POSITION * lights.data[pcb.idx].radius), 1.0f);
	gl_Position = camera.projection * camera.view * O_POSITION;
	O_UV0 = A_UV0;
}
