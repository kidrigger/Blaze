#version 450

#define KERNEL_SIZE 64
#define BLUR_KERNEL_SIZE 8

layout(location = 0) in vec4 V_POSITION;
layout(location = 1) in vec2 V_UV0;

layout(location = 0) out float O_COLOR;

layout(set = 0, binding = 0) uniform CameraUBO {
	mat4 view;
	mat4 projection;
	vec3 viewPos;
	float _pad;
	vec2 screenSize;
	float nearPlane;
	float farPlane;
} camera;

layout(set = 0, binding = 1) uniform SettingsUBO {
	int enableIBL;
	int viewRT;
} settings;

layout(push_constant) uniform BlurSettings {
	int blurEnable;
	int depthAwareBlur;
	float depth;
} pcb;

layout(set = 1, binding = 0) uniform sampler2D I_SSAO;
layout(set = 1, binding = 1) uniform sampler2D I_POSITION;

void main() {
	O_COLOR = texture(I_SSAO, V_UV0).r;
}
