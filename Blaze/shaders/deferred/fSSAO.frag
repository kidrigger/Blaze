#version 450

#define KERNEL_SIZE 64

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

layout(set = 1, binding = 0) uniform sampler2D I_NORMAL;
layout(set = 1, binding = 1) uniform sampler2D I_POSITION;

layout(set = 2, binding = 0) uniform KernelDataUBO {
	vec4 data[KERNEL_SIZE];
} kernel;
layout(set = 2, binding = 1) uniform sampler2D noise;

layout(push_constant) uniform PushConst {
	float radius;
	float bias;
} pcb;

void main() {
	vec2 noiseScale = camera.screenSize * 0.25f;
	
	vec3 fragPos	= texture(I_POSITION, V_UV0).xyz;
	vec3 normal		= texture(I_NORMAL, V_UV0).rgb;
	vec3 randomVec	= vec3(texture(noise, V_UV0 * noiseScale).xy, 0.0f);

	vec3 tangent	= normalize(randomVec - normal * dot(randomVec, normal));
	vec3 bitangent	= -cross(normal, tangent);
	mat3 TBN		= mat3(tangent, bitangent, normal);

	float occlusion	= 0.0;

	float minSamplePosZ = 0.0f;
	float minSampleDepth = 0.0f;

	vec3 viewFragPos = vec3(camera.view * vec4(fragPos, 1.0f));

	for(int i = 0; i < KERNEL_SIZE; ++i) {

		vec4 samplePos = vec4(TBN * kernel.data[i].xyz, 1.0f);
		samplePos = vec4(fragPos + samplePos.xyz * pcb.radius, 1.0f);
		
		samplePos = camera.view * vec4(samplePos);

		// project
		vec4 offset = camera.projection * samplePos;
		offset.xyz /= offset.w;
		offset.xyz = offset.xyz * 0.5f + 0.5f;
		offset.y = 1.0f - offset.y;
		
		float sampleDepth = -texture(I_POSITION, offset.xy).w;

		if (samplePos.z < minSamplePosZ) {
			minSamplePosZ = samplePos.z;
			minSampleDepth = sampleDepth;
		}

		float rangeCheck = smoothstep(0.0f, 1.0f, pcb.radius / abs(viewFragPos.z - sampleDepth));
		occlusion += (sampleDepth >= samplePos.z + pcb.bias ? 1.0f : 0.0f) * rangeCheck;
	}

	occlusion = 1.0f - (occlusion / float(KERNEL_SIZE));

	//ENDTEST

	O_COLOR = occlusion;
}
