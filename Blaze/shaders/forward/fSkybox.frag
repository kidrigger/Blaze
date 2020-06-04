#version 450

#define MANUAL_SRGB 1

layout(location = 0) in vec4 V_POSITION;
layout(location = 1) in vec4 V_NORMAL;
layout(location = 2, component = 0) in vec2 V_UV0;
layout(location = 2, component = 2) in vec2 V_UV1;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform CameraUBO {
	mat4 view;
	mat4 projection;
	vec3 viewPos;
	float farPlane;
} camera;

layout(set = 0, binding = 1) uniform SettingsUBO {
	float exposure;
	float gamma;
	int enableIBL;
} settings;

layout(set = 1, binding = 0) uniform samplerCube skybox;
layout(set = 1, binding = 1) uniform samplerCube irradianceMap;
layout(set = 1, binding = 2) uniform samplerCube prefilteredMap;
layout(set = 1, binding = 3) uniform sampler2D brdfLUT;

layout(push_constant) uniform ModelBlock {
	float only_for_compatibility_with_PBR_[32];
} pcb;

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
	outColor = vec4(SRGBtoLINEAR(tonemap(textureLod(skybox, V_POSITION.xyz, 0))).rgb, 1.0f);
}
