#version 450

#define KERNEL_SIZE 64

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

layout(set = 1, binding = 0) uniform sampler2D I_SSAO;

void main() {
	vec2 texelSize = 1.0 / vec2(textureSize(I_SSAO, 0));
	float result = 0.0;
	for (int x = -2; x < 2; ++x) 
	{
		for (int y = -2; y < 2; ++y) 
		{
			vec2 offset = vec2(float(x), float(y)) * texelSize;
			result += texture(I_SSAO, V_UV0 + offset).r;
		}
	}
	O_COLOR = result * 0.0625f; // ( 1.0f / 16.0f )
}
