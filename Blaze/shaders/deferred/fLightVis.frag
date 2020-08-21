#version 450

#define MAX_TEX_IN_MAT 32
#define MAX_POINT_LIGHTS 16
#define MAX_DIRECTION_LIGHTS 4
#define MAX_SHADOWS 16
#define MAX_CASCADES 4

#define MANUAL_SRGB 1

const uint RT_RENDER = 0x0;
const uint RT_POSITION = 0x1;
const uint RT_NORMAL = 0x2;
const uint RT_ALBEDO = 0x3;
const uint RT_AO = 0x4;
const uint RT_METALLIC = 0x5;
const uint RT_ROUGHNESS = 0x6;
const uint RT_EMISSION = 0x7;
const uint RT_IBL = 0x8;

layout(location = 0) in vec4 V_POSITION;
layout(location = 1) in vec4 V_NORMAL;
layout(location = 2, component = 0) in vec2 V_UV0;
layout(location = 2, component = 2) in vec2 V_UV1;

layout(location = 0) out vec4 O_COLOR;

layout(set = 0, binding = 0) uniform CameraUBO {
	mat4 view;
	mat4 projection;
	vec3 viewPos;
	float ambientBrightness;
	vec2 screenSize;
	float nearPlane;
	float farPlane;
} camera;

layout(set = 0, binding = 1) uniform SettingsUBO {
	int enableIBL;
	int viewRT;
} settings;

layout(push_constant) uniform ModelBlock {
	mat4 model;
	vec4 color;
} pcb;

void main()
{
	// Setup
	vec3 lightColor = vec3(23.47, 21.31, 20.79);

	if (settings.viewRT == RT_RENDER) {
		O_COLOR = vec4(lightColor * vec3(pcb.color), 1.0f);
	} else {
		discard;
	}
}
