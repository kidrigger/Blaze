#version 450

#define MANUAL_SRGB 1

const uint RT_POSITION = 0x0;
const uint RT_NORMAL = 0x1;
const uint RT_ALBEDO = 0x2;
const uint RT_OMR = 0x3;
const uint RT_EMISSION = 0x4;
const uint RT_RENDER = 0x5;

layout(location = 0) in vec4 V_POSITION;
layout(location = 1, component = 0) in vec2 V_UV0;

layout(location = 0) out vec4 O_COLOR;

layout(set = 0, binding = 0) uniform CameraUBO {
	mat4 view;
	mat4 projection;
	vec3 viewPos;
	float farPlane;
} camera;

layout(set = 0, binding = 1) uniform SettingsUBO {
	int viewRT;
} settings;

layout(input_attachment_index = 1, set = 1, binding = 0) uniform subpassInput I_POSITION;
layout(input_attachment_index = 2, set = 1, binding = 1) uniform subpassInput I_NORMAL;
layout(input_attachment_index = 3, set = 1, binding = 2) uniform subpassInput I_ALBEDO;
layout(input_attachment_index = 4, set = 1, binding = 3) uniform subpassInput I_OMR;
layout(input_attachment_index = 5, set = 1, binding = 4) uniform subpassInput I_EMISSION;

void main() {
	switch (settings.viewRT) {
		case RT_POSITION: O_COLOR = vec4(subpassLoad(I_POSITION).rgb, 1.0f); break;
		case RT_NORMAL: O_COLOR = vec4(subpassLoad(I_NORMAL).rgb, 1.0f); break;
		case RT_ALBEDO: O_COLOR = vec4(subpassLoad(I_ALBEDO).rgb, 1.0f); break;
		case RT_OMR: O_COLOR = vec4(subpassLoad(I_OMR).rgb, 1.0f); break;
		case RT_EMISSION: O_COLOR = vec4(subpassLoad(I_EMISSION).rgb, 1.0f); break;
		default: O_COLOR = vec4(subpassLoad(I_POSITION).rgb, 1.0f); break;
	}
}
