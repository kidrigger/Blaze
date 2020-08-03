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
	vec2 ssaoTexelSize = 1.0 / vec2(textureSize(I_SSAO, 0));

	if (pcb.blurEnable > 0) {
		float result = 0.0;

		float depth = texture(I_POSITION, V_UV0).a;
		float total = 0.0f;
		for (int x = -BLUR_KERNEL_SIZE; x <= BLUR_KERNEL_SIZE; ++x) 
		{
			for (int y = -BLUR_KERNEL_SIZE; y <= BLUR_KERNEL_SIZE; ++y) 
			{
				vec2 offset = vec2(float(x), float(y)) * ssaoTexelSize;

				float factor = 1.0f;
				if (pcb.depthAwareBlur > 0) {
					float deltaDepth = abs(texture(I_POSITION, V_UV0 + offset).a - depth);
					factor = step(deltaDepth, pcb.depth);
				}

				result += texture(I_SSAO, V_UV0 + offset).r * factor;
				total += factor;
			}
		}
		O_COLOR = result / total;
	} else {
		O_COLOR = texture(I_SSAO, V_UV0).r;
	}
}
