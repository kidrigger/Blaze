#version 450

#define MAX_TEX_IN_MAT 32
#define MAX_POINT_LIGHTS 16
#define MAX_DIRECTION_LIGHTS 4
#define MAX_SHADOWS 16
#define MAX_CASCADES 4

#define MANUAL_SRGB 1

layout(location = 0) in vec4 V_POSITION;
layout(location = 1, component = 0) in vec2 V_UV0;

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

layout(push_constant) uniform LightIdx {
	int idx;
	int pad0_;
	int pad1_;
	int pad2_;
} pcb;

const float PI = 3.1415926535897932384626433832795f;

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

float getPointShadow(int lightIdx, vec3 N, vec3 position) {
	int shadowIdx = lights.data[lightIdx].shadowIndex;
	if (shadowIdx < 0) {
		return 0.0f;
	}
	vec3 dir = position - lights.data[lightIdx].position;
	dir.x *= -1;
	float current_depth = length(dir);
	dir = normalize(dir);
	float closest_depth = texture(shadows[shadowIdx], dir).r * lights.data[lightIdx].radius;
	float shadow_bias = max(0.05f * (1.0f - dot(N, dir)), 0.005f);
	return ((current_depth - shadow_bias) > closest_depth ? 1.0f: 0.0f);
}

void main() {
	vec2 UV = gl_FragCoord.xy / camera.screenSize;

	vec3 lightColor = vec3(23.47, 21.31, 20.79);
	vec3 position = texture(I_POSITION, UV).rgb;
	vec3 N = texture(I_NORMAL, UV).rgb;
	vec3 OMR = texture(I_OMR, UV).rgb;
	float ao = OMR.r;
	float metallic = OMR.g;
	float roughness = OMR.b;
	vec3 albedo = texture(I_ALBEDO, UV).rgb;
	vec3 V = normalize(camera.viewPos - position.xyz);

	int i = pcb.idx;
	float d = distance(position, lights.data[i].position);

	
	vec3 F0 = vec3(0.04);
	F0		= mix(F0, albedo, metallic);

	vec3 L0 = vec3(0.0f);

	if (d >= lights.data[i].radius) discard;
		
	vec3 L		 = normalize(lights.data[i].position.xyz - position.xyz);
	vec3 H		 = normalize(V + L);
	float cosine = max(dot(L, N), 0.0f);

	float dist		  = length(lights.data[i].position.xyz - position.xyz);
	float attenuation = 1.0 / (dist * dist);
	vec3 radiance	  = lightColor * attenuation * lights.data[i].brightness;
		
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
	float shade = getPointShadow(i, N, position);

	L0 += mix((kd * albedo / PI + specular) * radiance * NdotL, vec3(0.0f), shade);

	O_COLOR = vec4(L0, 1.0f);
}
