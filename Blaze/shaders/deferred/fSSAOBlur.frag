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
	float ambientBrightness;
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

const float weight[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
	vec2 ssaoTexelSize = 1.0 / vec2(textureSize(I_SSAO, 0));

//	if (pcb.blurEnable > 0) {
//		float result = 0.0;
//
//		float depth = texture(I_POSITION, V_UV0).a;
//		float total = 0.0f;
//		for (int x = -BLUR_KERNEL_SIZE; x <= BLUR_KERNEL_SIZE; ++x) 
//		{
//			for (int y = -BLUR_KERNEL_SIZE; y <= BLUR_KERNEL_SIZE; ++y) 
//			{
//				vec2 offset = vec2(float(x), float(y)) * ssaoTexelSize;
//
//				float factor = 1.0f;
//				if (pcb.depthAwareBlur > 0) {
//					float deltaDepth = abs(texture(I_POSITION, V_UV0 + offset).a - depth);
//					factor = step(deltaDepth, pcb.depth);
//				}
//
//				result += texture(I_SSAO, V_UV0 + offset).r * factor;
//				total += factor;
//			}
//		}
//		O_COLOR = result / total;
//	} else {
//		O_COLOR = texture(I_SSAO, V_UV0).r;
//	}

	float result = texture(I_SSAO, V_UV0).r * weight[0];
	float p0 = texture(I_SSAO, V_UV0).r;
	float total = 0.0f;
	float depth = texture(I_POSITION, V_UV0).a;
	if (pcb.blurEnable > 0) {
		for (int i = 1; i < 5; ++i) {
			vec2 offset = vec2(ssaoTexelSize.x * i, 0.0f);
			vec2 factor = vec2(1.0f);
			if (pcb.depthAwareBlur > 0) {
				float deltaDepth = abs(texture(I_POSITION, V_UV0 + offset).a - depth);
				factor.x = step(deltaDepth, pcb.depth);
				deltaDepth = abs(texture(I_POSITION, V_UV0 - offset).a - depth);
				factor.y = step(deltaDepth, pcb.depth);
			}
			result += mix(p0, texture(I_SSAO, V_UV0 + offset).r, factor.x) * weight[i];
			result += mix(p0, texture(I_SSAO, V_UV0 - offset).r, factor.y) * weight[i];
		}
	} else {
		for (int i = 1; i < 5; ++i) {
			vec2 offset = vec2(0.0f, ssaoTexelSize.y * i);
			vec2 factor = vec2(1.0f);
			if (pcb.depthAwareBlur > 0) {
				float deltaDepth = abs(texture(I_POSITION, V_UV0 + offset).a - depth);
				factor.x = step(deltaDepth, pcb.depth);
				deltaDepth = abs(texture(I_POSITION, V_UV0 - offset).a - depth);
				factor.y = step(deltaDepth, pcb.depth);
			}
			result += mix(p0, texture(I_SSAO, V_UV0 + offset).r, factor.x) * weight[i];
			result += mix(p0, texture(I_SSAO, V_UV0 - offset).r, factor.y) * weight[i];
		}
	}

	O_COLOR = result;
}
