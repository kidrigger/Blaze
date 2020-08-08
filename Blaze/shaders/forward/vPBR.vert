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
layout(location = 3) out vec4 O_VIEWPOS;
layout(location = 4) out vec4 O_LIGHTCOORD[MAX_DIRECTION_LIGHTS][MAX_CASCADES];

layout(set = 0, binding = 0) uniform CameraUBO {
	mat4 view;
	mat4 projection;
	vec3 viewPos;
	float ambientBrightness;
	vec2 screenSize;
	float nearPlane;
	float farPlane;
} camera;

struct DirLightData {
	vec3 direction;
	float brightness;
	vec4 cascadeSplits;
	mat4 cascadeViewProj[MAX_CASCADES];
	int numCascades;
	int shadowIndex;
};

layout(set = 3, binding = 1) uniform DirLightUBO {
	DirLightData data[MAX_DIRECTION_LIGHTS];
} dirLights;

layout(push_constant) uniform ModelBlock {
	mat4 model;
	float opaque_[18];
} pcb;

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

void main() {
	O_POSITION = pcb.model * vec4(A_POSITION, 1.0f);
	O_VIEWPOS = camera.view * O_POSITION;
	gl_Position = camera.projection * O_VIEWPOS;
	O_NORMAL = transpose(inverse(pcb.model)) * vec4(A_NORMAL, 0.0f);
	O_UV0 = A_UV0;
	O_UV1 = A_UV1;
	for (int i = 0; i < MAX_DIRECTION_LIGHTS; ++i) {
		for (int cascade = 0; cascade < MAX_CASCADES; ++cascade) {
			O_LIGHTCOORD[i][cascade] = biasMat * dirLights.data[i].cascadeViewProj[cascade] * O_POSITION;
			O_LIGHTCOORD[i][cascade].y = 1.0f - O_LIGHTCOORD[i][cascade].y;
		}
	}
}
