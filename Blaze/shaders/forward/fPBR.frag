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
layout(location = 3) in vec4 V_VIEWPOS;
layout(location = 4) in vec4 V_LIGHTCOORD[MAX_DIRECTION_LIGHTS][MAX_CASCADES];

layout(location = 0) out vec4 outColor;

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
	float exposure;
	float gamma;
	int enableIBL;
} settings;

layout(set = 1, binding = 0) uniform samplerCube skybox;
layout(set = 1, binding = 1) uniform samplerCube irradianceMap;
layout(set = 1, binding = 2) uniform samplerCube prefilteredMap;
layout(set = 1, binding = 3) uniform sampler2D brdfLUT;

layout(set = 2, binding = 0) uniform sampler2D diffuseMap[MAX_TEX_IN_MAT];
layout(set = 2, binding = 1) uniform sampler2D normalMap[MAX_TEX_IN_MAT];
layout(set = 2, binding = 2) uniform sampler2D metalRoughMap[MAX_TEX_IN_MAT];
layout(set = 2, binding = 3) uniform sampler2D occlusionMap[MAX_TEX_IN_MAT];
layout(set = 2, binding = 4) uniform sampler2D emissionMap[MAX_TEX_IN_MAT];

struct PointLightData {
	vec3 position;
	float radius;
	vec3 color;
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

// AlphaMode
const uint ALPHA_OPAQUE = 0x00000000u;
const uint ALPHA_MASK   = 0x00000001u;
const uint ALPHA_BLEND  = 0x00000002u;

layout(set = 3, binding = 0) uniform PointLightUBO {
	PointLightData data[MAX_POINT_LIGHTS];
} lights;
layout(set = 3, binding = 1) uniform DirLightUBO {
	DirLightData data[MAX_DIRECTION_LIGHTS];
} dirLights;

layout(set = 4, binding = 0) uniform samplerCube shadows[MAX_SHADOWS];
layout(set = 4, binding = 1) uniform sampler2DArray dirShadows[MAX_SHADOWS];

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
	vec3 outcol = Uncharted2Tonemap(color.rgb * settings.exposure);
	outcol = outcol * (1.0f / Uncharted2Tonemap(vec3(11.2f)));	
	return vec4(pow(outcol, vec3(1.0f / settings.gamma)), color.a);
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

float getPointShadow(int lightIdx, vec3 N) {
	int shadowIdx = lights.data[lightIdx].shadowIndex;
	if (shadowIdx < 0) {
		return 0.0f;
	}
	vec3 dir = V_POSITION.xyz - lights.data[lightIdx].position;
	dir.x *= -1;
	float current_depth = length(dir);
	dir = normalize(dir);
	float closest_depth = texture(shadows[shadowIdx], dir).r * lights.data[lightIdx].radius;
	float shadow_bias = max(0.05f * (1.0f - dot(N, dir)), 0.005f);
	return ((current_depth - shadow_bias) > closest_depth ? 1.0f: 0.0f);
}

float getDirectionShadow(int lightIdx, vec3 N) {
	int shadowIdx = dirLights.data[lightIdx].shadowIndex;
	if (shadowIdx < 0) {
		return 0.0f;
	}

	int cascade = 0;
	for (int i = 0; i < dirLights.data[lightIdx].numCascades; i++) {
		if (-V_VIEWPOS.z > dirLights.data[lightIdx].cascadeSplits[i]) {
			cascade = i+1;
		}
	}

	vec4 shadowCoord = V_LIGHTCOORD[shadowIdx][cascade];
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

void main()
{
	// Setup
	vec3 lightColor = vec3(23.47, 21.31, 20.79);
	vec3 N = (pcb.normalTextureSet > -1 ? getNormal() : normalize(V_NORMAL.xyz));
	vec3 V = normalize(camera.viewPos - V_POSITION.xyz);

	vec3 albedo;
	float alpha;
	float metallic;
	float roughness;
	float ao;
	vec3 emission;

	if (pcb.baseColorTextureSet < 0) {
		albedo = pcb.baseColorFactor.rgb;
		alpha = pcb.baseColorFactor.a;
	} else {
		vec4 texRGBA = texture(diffuseMap[pcb.textureArrIdx], V_UV0);
		albedo = SRGBtoLINEAR(texRGBA).rgb * pcb.baseColorFactor.rgb;
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
		metallic  = pcb.metallicFactor;
		roughness = pcb.roughnessFactor;
		ao		  = 1.0f;
	} else {
		vec3 metalRough = texture(metalRoughMap[pcb.textureArrIdx], V_UV0).rgb;
		metallic		= metalRough.b * pcb.metallicFactor;
		roughness		= metalRough.g * pcb.roughnessFactor;
		ao				= 1.0f;
	}

	if (pcb.occlusionTextureSet >= 0) {
		ao = texture(occlusionMap[pcb.textureArrIdx], V_UV0).r;
	}

	if (pcb.emissiveTextureSet < 0) {
		emission = vec3(0.0f);
	} else {
		emission = SRGBtoLINEAR(texture(emissionMap[pcb.textureArrIdx], V_UV0)).rgb * pcb.emissiveColorFactor.rgb;
	}

	// Lighting setup

	vec3 F0 = vec3(0.04);
	F0		= mix(F0, albedo, metallic);

	vec3 L0 = vec3(0.0f);

	vec3 ambient = vec3(0.03f) * ao * albedo;

	// Point Lighting
	for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
		if (lights.data[i].radius < 0.0f) continue;
		if (distance(V_POSITION.xyz, lights.data[i].position) >= lights.data[i].radius) continue;
		
		vec3 L		 = normalize(lights.data[i].position.xyz - V_POSITION.xyz);
		vec3 H		 = normalize(V + L);
		float cosine = max(dot(L, N), 0.0f);

		float dist		  = length(lights.data[i].position.xyz - V_POSITION.xyz);
		float attenuation = 1.0 / (dist * dist);
		vec3 radiance	  = lightColor * attenuation * lights.data[i].color;
		
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
		float shade = getPointShadow(i, N);

		L0 += mix((kd * albedo / PI + specular) * radiance * NdotL, vec3(0.0f), shade);
	}

	// Direction Lighting
	for (int i = 0; i < MAX_DIRECTION_LIGHTS; i++) {
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
		float shade = getDirectionShadow(i, N);

		L0 += mix((kd * albedo / PI + specular) * radiance * NdotL, vec3(0.0f), shade);
	}

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

		ambient = (kd * diffuse + specular) * ao;
	} else {
		vec3 F		  = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
	
		vec3 ks = F;
		vec3 kd = vec3(1.0f) - ks;
		kd *= 1.0f - metallic;

		vec3 diffuse = camera.ambientBrightness * albedo;

		ambient = (kd * diffuse) * ao;
	}

	vec3 color	 = ambient + L0 + emission;
	outColor	 = SRGBtoLINEAR(tonemap(vec4(color, alpha)));
}
