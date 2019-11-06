#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoords0;
layout(location = 3) in vec2 texCoords1;
layout(location = 4) in vec3 inColor;

struct Light {
	vec3 position;
	int type;
	vec3 direction;
};

layout(set = 0, binding = 0) uniform UniformBufferObject {
	mat4 view;
	mat4 projection;
	vec3 viewPos;
	int numLights;
	vec4 lightPos[16];
} ubo;

layout(set = 0, binding = 1) uniform SettingsUBO {
	int viewMap;
	int skyboxEnabled;
	int enableIBL;
} debugSettings;

layout(set = 1, binding = 0) uniform sampler2D diffuseImage;
layout(set = 1, binding = 1) uniform sampler2D metalRoughnessImage;
layout(set = 1, binding = 2) uniform sampler2D normalImage;
layout(set = 1, binding = 3) uniform sampler2D occlusionImage;
layout(set = 1, binding = 4) uniform sampler2D emissiveImage;

layout(set = 2, binding = 0) uniform samplerCube skybox;
layout(set = 2, binding = 1) uniform samplerCube irradianceMap;
layout(set = 2, binding = 2) uniform samplerCube prefilteredMap;
layout(set = 2, binding = 3) uniform sampler2D brdfLUT;

layout(push_constant) uniform MaterialData {
	layout(offset = 64) vec4 baseColorFactor;
	vec4 emissiveColorFactor;
	float metallicFactor;
	float roughnessFactor;
	int baseColorTextureSet;
	int physicalDescriptorTextureSet;
	int normalTextureSet;
	int occlusionTextureSet;
	int emissiveTextureSet;
} material;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359f;

vec3 Uncharted2Tonemap(vec3 color)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	float W = 11.2;
	return ((color*(A*color+C*B)+D*E)/(color*(A*color+B)+D*F))-E/F;
}

vec4 tonemap(vec4 color)
{
	vec3 outcol = Uncharted2Tonemap(color.rgb * 4.5f);
	outcol = outcol * (1.0f / Uncharted2Tonemap(vec3(11.2f)));	
	return vec4(pow(outcol, vec3(1.0f / 2.2f)), color.a);
}

#define MANUAL_SRGB 1

vec4 SRGBtoLINEAR(vec4 srgbIn)
{
	#ifdef MANUAL_SRGB
	#ifdef SRGB_FAST_APPROXIMATION
	vec3 linOut = pow(srgbIn.xyz,vec3(2.2));
	#else //SRGB_FAST_APPROXIMATION
	vec3 bLess = step(vec3(0.04045),srgbIn.xyz);
	vec3 linOut = mix( srgbIn.xyz/vec3(12.92), pow((srgbIn.xyz+vec3(0.055))/vec3(1.055),vec3(2.4)), bLess );
	#endif //SRGB_FAST_APPROXIMATION
	return vec4(linOut,srgbIn.w);
	#else //MANUAL_SRGB
	return srgbIn;
	#endif //MANUAL_SRGB
}

void main() {
	outColor = (debugSettings.skyboxEnabled > 0) ? vec4(SRGBtoLINEAR(tonemap(textureLod(skybox, position, 0))).rgb, 1.0f) : vec4(0.0f, 0.0f, 0.0f, 1.0f);
}
