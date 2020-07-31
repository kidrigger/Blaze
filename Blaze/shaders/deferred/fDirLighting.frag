#version 450

#define MAX_TEX_IN_MAT 32
#define MAX_POINT_LIGHTS 16
#define MAX_DIRECTION_LIGHTS 4
#define MAX_SHADOWS 16
#define MAX_CASCADES 4

#define MANUAL_SRGB 1

const uint RT_RENDER = 0x0;
const uint RT_POSITION = 0x1;
const uint RT_NORMAL = 0x2;
const uint RT_ALBEDO = 0x3;
const uint RT_AO = 0x4;
const uint RT_METALLIC = 0x5;
const uint RT_ROUGHNESS = 0x6;
const uint RT_EMISSION = 0x7;
const uint RT_IBL = 0x8;

layout(location = 0) in vec4 V_POSITION;

layout(location = 0) out vec4 O_COLOR;

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

layout(set = 1, binding = 0) uniform sampler2D I_POSITION;
layout(set = 1, binding = 1) uniform sampler2D I_NORMAL;
layout(set = 1, binding = 2) uniform sampler2D I_ALBEDO;
layout(set = 1, binding = 3) uniform sampler2D I_OMR;
layout(set = 1, binding = 4) uniform sampler2D I_EMISSION;

layout(set = 2, binding = 0) readonly buffer Lights {
	PointLightData data[];
} lights;

layout(set = 2, binding = 1) readonly buffer DirLights {
	DirLightData data[];
} dirLights;

layout(set = 3, binding = 0) uniform samplerCube shadows[MAX_SHADOWS];
layout(set = 3, binding = 1) uniform sampler2DArray dirShadows[MAX_SHADOWS];

layout(set = 4, binding = 0) uniform samplerCube skybox;
layout(set = 4, binding = 1) uniform samplerCube irradianceMap;
layout(set = 4, binding = 2) uniform samplerCube prefilteredMap;
layout(set = 4, binding = 3) uniform sampler2D brdfLUT;

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

vec4 sampleSkybox() {
	return vec4(texture(skybox, normalize(V_POSITION.xyz)).rgb, 1.0f);
}

void main() {
	vec2 UV = gl_FragCoord.xy / camera.screenSize;

	vec3 lightColor = vec3(23.47, 21.31, 20.79);
	vec4 hCoord = texture(I_POSITION, UV);
	vec3 position = hCoord.xyz;
	float pixelValid = hCoord.a;
	vec3 N = texture(I_NORMAL, UV).rgb;
	vec3 OMR = texture(I_OMR, UV).rgb;
	float ao = OMR.r;
	float metallic = OMR.g;
	float roughness = OMR.b;
	vec3 albedo = texture(I_ALBEDO, UV).rgb;
	vec3 emission = texture(I_EMISSION, UV).rgb;
	vec3 V = normalize(camera.viewPos - position.xyz);

	// Skybox out
	if (pixelValid < 0.01f) {
		O_COLOR = sampleSkybox();
		return;
	}

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

	vec3 iblContrib = vec3(0.0f);
	if (settings.enableIBL > 0) {
		vec3 R = reflect(-V, N);

		const float MAX_REFLECTION_LOD = 4.0f;
		vec3 prefilteredColor = textureLod(prefilteredMap, R,  roughness * MAX_REFLECTION_LOD).rgb;

		vec3 F		  = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
		vec2 envBRDF  = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
		vec3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);
	
		vec3 ks = F;
		vec3 kd = vec3(1.0f) - ks;
		kd *= 1.0f - metallic;

		vec3 diffuse = texture(irradianceMap, N).rgb * albedo;

		iblContrib = (kd * diffuse + specular);
	}

	ambient = iblContrib * ao;

	switch (settings.viewRT) {
		case RT_RENDER: O_COLOR = vec4(ambient + L0 + emission, 1.0f); break;
		case RT_POSITION: O_COLOR = vec4(position, 1.0f); break;
		case RT_NORMAL: O_COLOR = vec4(N, 1.0f); break;
		case RT_ALBEDO: O_COLOR = vec4(albedo, 1.0f); break;
		case RT_AO: O_COLOR = vec4(OMR.rrr, 1.0f); break;
		case RT_METALLIC: O_COLOR = vec4(OMR.ggg, 1.0f); break;
		case RT_ROUGHNESS: O_COLOR = vec4(OMR.bbb, 1.0f); break;
		case RT_EMISSION: O_COLOR = vec4(emission, 1.0f); break;
		case RT_IBL: O_COLOR = vec4(iblContrib, 1.0f); break;
	}
}
