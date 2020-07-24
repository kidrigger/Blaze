#version 450

#define MAX_TEX_IN_MAT 32
#define MAX_POINT_LIGHTS 16
#define MAX_DIRECTION_LIGHTS 4
#define MAX_SHADOWS 16
#define MAX_CASCADES 4

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


struct PointLightData {
	vec3 position;
	float brightness;
	float radius;
	int shadowIndex;
};

struct DirLightData {
	vec3 direction;
	float brightness;
	vec4 cascadeSplits;
	mat4 cascadeViewProj[MAX_CASCADES];
	int numCascades;
	int shadowIndex;
};

layout(set = 1, binding = 0) readonly buffer Lights {
	PointLightData data[];
} lights;

layout(set = 1, binding = 1) readonly buffer DirLights {
	DirLightData data[];
} dirLights;

layout(input_attachment_index = 1, set = 2, binding = 0) uniform subpassInput I_POSITION;
layout(input_attachment_index = 2, set = 2, binding = 1) uniform subpassInput I_NORMAL;
layout(input_attachment_index = 3, set = 2, binding = 2) uniform subpassInput I_ALBEDO;
layout(input_attachment_index = 4, set = 2, binding = 3) uniform subpassInput I_OMR;
layout(input_attachment_index = 5, set = 2, binding = 4) uniform subpassInput I_EMISSION;

layout(set = 3, binding = 0) uniform samplerCube shadows[MAX_SHADOWS];
layout(set = 3, binding = 1) uniform sampler2DArray dirShadows[MAX_SHADOWS];

layout(push_constant) uniform LightIdx {
	int idx;
	int pad0_;
	int pad1_;
	int pad2_;
} pcb;

const float PI = 3.1415926535897932384626433832795f;

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
	float a		 = roughness*roughness;
	float a2	 = a*a;
	float NdotH  = max(dot(N, H), 0.0);
	float NdotH2 = NdotH*NdotH;
	
	float num	= a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom		= PI * denom * denom;
	
	return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = roughness + 1.0f;
	float k = (r * r) / 8.0f;

	float num   = NdotV;
	float denom = NdotV * (1.0 - k) + k;
	
	return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2  = GeometrySchlickGGX(NdotV, roughness);
	float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
	return ggx1 * ggx2;
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
	return F0 + (max(vec3(1.0f - roughness), F0) - F0) * pow(1.0f - cosTheta, 5.0f);
}

float getDirectionShadow(int lightIdx, vec3 N, vec3 position) {
	int shadowIdx = dirLights.data[lightIdx].shadowIndex;
	if (shadowIdx < 0) {
		return 0.0f;
	}

	vec4 viewpos = camera.view * vec4(position, 1.0f);

	int cascade = 0;
	for (int i = 0; i < dirLights.data[lightIdx].numCascades; i++) {
		if (-viewpos.z > dirLights.data[lightIdx].cascadeSplits[i]) {
			cascade = i+1;
		}
	}

	vec4 shadowCoord = biasMat * dirLights.data[shadowIdx].cascadeViewProj[cascade] * vec4(position, 1.0f);// V_LIGHTCOORD[shadowIdx][cascade];
	shadowCoord.y = 1.0f - shadowCoord.y;
	float shade = 0.0f;

	vec2 texelSize = vec2(1.0f) / textureSize(dirShadows[shadowIdx], 0).xy;
	for (int i = -1; i <= 1; i++)
	{
		for (int j = -1; j <= 1; j++)
		{
			if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) 
			{
				float dist = texture( dirShadows[shadowIdx], vec3(shadowCoord.st + vec2(i,j) * texelSize, cascade)).r;
				if ( shadowCoord.w > 0.0 && dist < shadowCoord.z )
				{
					shade += 1.0f;
				}
			}
		}
	}
	shade /= 9.0f;
	return shade;
}

void main() {
	vec3 lightColor = vec3(23.47, 21.31, 20.79);
	vec3 position = subpassLoad(I_POSITION).rgb;
	vec3 N = subpassLoad(I_NORMAL).rgb;
	vec3 OMR = subpassLoad(I_OMR).rgb;
	float ao = OMR.r;
	float metallic = OMR.g;
	float roughness = OMR.b;
	vec3 albedo = subpassLoad(I_ALBEDO).rgb;
	vec3 emission = subpassLoad(I_EMISSION).rgb;
	vec3 V = normalize(camera.viewPos - position.xyz);

	// Lighting setup

	vec3 F0 = vec3(0.04);
	F0		= mix(F0, albedo, metallic);

	vec3 L0 = vec3(0.0f);

	vec3 ambient = vec3(0.03f) * albedo * ao;

	// Direction Lighting
	for (int i = 0; i < dirLights.data.length(); i++) {
		if (dirLights.data[i].brightness < 0.0f) continue;
		
		vec3 L		 = normalize(-dirLights.data[i].direction.xyz);
		vec3 H		 = normalize(V + L);
		float cosine = max(dot(L, N), 0.0f);

		vec3 radiance = lightColor * 0.25f * dirLights.data[i].brightness;
		
		float NDF = DistributionGGX(N, H, roughness);
		float G	  = GeometrySmith(N, V, L, roughness);
		vec3 F	  = fresnelSchlickRoughness(max(dot(H, V), 0.0f), F0, roughness);

		vec3 ks = F;
		vec3 kd = vec3(1.0f) - ks;
		kd *= 1.0f - metallic;

		vec3 numerator	  = NDF * G * F;
		float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
		vec3 specular	  = numerator / max(denominator, 0.001);

		float NdotL = max(dot(N, L), 0.0f);
		float shade = getDirectionShadow(i, N, position);

		L0 += mix((kd * albedo / PI + specular) * radiance * NdotL, vec3(0.0f), shade);
	}

//	if (settings.enableIBL > 0) {
//		vec3 R = reflect(-V, N);
//
//		const float MAX_REFLECTION_LOD = 4.0f;
//		vec3 prefilteredColor = textureLod(prefilteredMap, R,  roughness * MAX_REFLECTION_LOD).rgb;
//
//		vec3 F		  = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
//		vec2 envBRDF  = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
//		vec3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);
//	
//		vec3 ks = F;
//		vec3 kd = vec3(1.0f) - ks;
//		kd *= 1.0f - metallic;
//
//		vec3 diffuse = texture(irradianceMap, N).rgb * albedo;
//
//		ambient = (kd * diffuse + specular) * ao;
//	}

	O_COLOR	= vec4(ambient + L0 + emission, 1.0f);

	switch (settings.viewRT) {
		case RT_POSITION: O_COLOR = vec4(position, 1.0f); break;
		case RT_NORMAL: O_COLOR = vec4(N, 1.0f); break;
		case RT_ALBEDO: O_COLOR = vec4(albedo, 1.0f); break;
		case RT_OMR: O_COLOR = vec4(OMR, 1.0f); break;
		case RT_EMISSION: O_COLOR = vec4(emission, 1.0f); break;
	}
}
