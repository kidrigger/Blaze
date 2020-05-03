#version 450

#define MAX_TEX_IN_MAT 16

const float shadow_bias = 0.05f;

layout(location = 0) in vec3 V_POSITION;
layout(location = 1) in vec3 V_NORMAL;
layout(location = 2, component = 0) in vec2 V_UV0;
layout(location = 2, component = 2) in vec2 V_UV1;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D diffuseMap[MAX_TEX_IN_MAT];
layout(set = 1, binding = 1) uniform sampler2D normalMap[MAX_TEX_IN_MAT];
layout(set = 1, binding = 2) uniform sampler2D metalRoughMap[MAX_TEX_IN_MAT];
layout(set = 1, binding = 3) uniform sampler2D occlusionMap[MAX_TEX_IN_MAT];
layout(set = 1, binding = 4) uniform sampler2D emissionMap[MAX_TEX_IN_MAT];

layout(push_constant) uniform ModelBlock {
	mat4 model;
	vec4 baseColorFactor;
	vec4 emissiveColorFactor;
	float metallicFactor;
	float roughnessFactor;
	int baseColorTextureSet;
	int physicalDescriptorTextureSet;
	int normalTextureSet;
	int occlusionTextureSet;
	int emissiveTextureSet;
	int textureArrIdx;
} pcb;

void main()
{
	outColor = texture(diffuseMap[pcb.textureArrIdx], (pcb.baseColorTextureSet == 0 ? V_UV0 : V_UV1));
}
