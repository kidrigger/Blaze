#version 450

#define MAX_TEX_IN_MAT 32
#define MAX_POINT_LIGHTS 16
#define MAX_DIRECTION_LIGHTS 4
#define MAX_SHADOWS 16
#define MAX_CASCADES 4

#define MANUAL_SRGB 1

layout(location = 0) in vec4 V_POSITION;
layout(location = 1) in vec4 V_NORMAL;
layout(location = 2, component = 0) in vec2 V_UV0;
layout(location = 2, component = 2) in vec2 V_UV1;

layout(location = 0) out vec4 O_POSITION;
layout(location = 1) out vec4 O_NORMAL;
layout(location = 2) out vec4 O_ALBEDO;
layout(location = 3) out vec4 O_OMR;
layout(location = 4) out vec4 O_EMISSION;

layout(set = 0, binding = 0) uniform CameraUBO {
	mat4 view;
	mat4 projection;
	vec3 viewPos;
	float farPlane;
} camera;

layout(set = 0, binding = 1) uniform SettingsUBO {
	int viewRT;
} settings;

layout(set = 1, binding = 0) uniform sampler2D diffuseMap[MAX_TEX_IN_MAT];
layout(set = 1, binding = 1) uniform sampler2D normalMap[MAX_TEX_IN_MAT];
layout(set = 1, binding = 2) uniform sampler2D metalRoughMap[MAX_TEX_IN_MAT];
layout(set = 1, binding = 3) uniform sampler2D occlusionMap[MAX_TEX_IN_MAT];
layout(set = 1, binding = 4) uniform sampler2D emissionMap[MAX_TEX_IN_MAT];

// AlphaMode
const uint ALPHA_OPAQUE = 0x00000000u;
const uint ALPHA_MASK   = 0x00000001u;
const uint ALPHA_BLEND  = 0x00000002u;

layout(push_constant) uniform ModelBlock {
	float opaque_[16];
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
	int alphaMode;
	float alphaCutoff;
} pcb;

const float PI = 3.1415926535897932384626433832795f;

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

vec3 getNormal()
{
	vec3 tangentNormal = texture(normalMap[pcb.textureArrIdx], pcb.normalTextureSet == 0 ? V_UV0 : V_UV1).xyz * 2.0 - 1.0;

	vec4 q1  = dFdx(V_POSITION);
	vec4 q2  = dFdy(V_POSITION);
	vec2 st1 = dFdx(V_UV0);
	vec2 st2 = dFdy(V_UV0);

	vec3 N	 = normalize(V_NORMAL.xyz);
	vec3 T	 = normalize(q1 * st2.t - q2 * st1.t).xyz;
	vec3 B	 = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangentNormal);
}

void main()
{
	// Setup
	O_POSITION = V_POSITION;

	O_NORMAL = vec4((pcb.normalTextureSet > -1 ? getNormal() : normalize(V_NORMAL.xyz)), 0.0f);

	float alpha;
	if (pcb.baseColorTextureSet < 0) {
		O_ALBEDO = vec4(pcb.baseColorFactor.rgb, 1.0f);
		alpha = pcb.baseColorFactor.a;
	} else {
		vec4 texRGBA = texture(diffuseMap[pcb.textureArrIdx], V_UV0);
		O_ALBEDO = vec4(SRGBtoLINEAR(texRGBA).rgb * pcb.baseColorFactor.rgb, 1.0f);
		alpha = texRGBA.a;
	}

	if (pcb.alphaMode == ALPHA_MASK) {
		if (alpha < pcb.alphaCutoff) {
			discard;
		} else {
			alpha = 1.0f;
		}
	} else if (pcb.alphaMode == ALPHA_OPAQUE) {
		alpha = 1.0f;
	}

	if (pcb.physicalDescriptorTextureSet < 0) {
		O_OMR.g	= pcb.metallicFactor;
		O_OMR.b	= pcb.roughnessFactor;
		O_OMR.r	= 1.0f;
	} else {
		vec3 metalRough = texture(metalRoughMap[pcb.textureArrIdx], V_UV0).rgb;
		O_OMR.g			= metalRough.b * pcb.metallicFactor;
		O_OMR.b			= metalRough.g * pcb.roughnessFactor;
		O_OMR.r			= 1.0f;
	}

	if (pcb.occlusionTextureSet >= 0) {
		O_OMR.r = texture(occlusionMap[pcb.textureArrIdx], V_UV0).r;
	}

	if (pcb.emissiveTextureSet < 0) {
		O_EMISSION = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	} else {
		O_EMISSION = vec4(SRGBtoLINEAR(texture(emissionMap[pcb.textureArrIdx], V_UV0)).rgb * pcb.emissiveColorFactor.rgb, 1.0f);
	}
}
