#version 450

const float shadow_bias = 0.05f;

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

layout(set = 0, binding = 0) uniform CameraBufferObject {
	mat4 view;
	mat4 projection;
	vec3 viewPos;
	int numLights;
	vec4 lightPos[16];
} ubo;

layout(set = 0, binding = 1) uniform SettingsUBO {
	int viewMap;
	int enableSkybox;
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

layout(set = 3, binding = 0) uniform samplerCube shadow;

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
	vec3 outcol = Uncharted2Tonemap(color.rgb);
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

vec3 getNormal()
{
	vec3 tangentNormal = texture(normalImage, material.normalTextureSet == 0 ? texCoords0 : texCoords1).xyz * 2.0 - 1.0;

	vec3 q1  = dFdx(position);
	vec3 q2  = dFdy(position);
	vec2 st1 = dFdx(texCoords0);
	vec2 st2 = dFdy(texCoords0);

	vec3 N	 = normalize(normal);
	vec3 T	 = normalize(q1 * st2.t - q2 * st1.t);
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
	float r = roughness;
	float k = (r * r) / 2.0;

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

float calculateShadow(int lightIdx) {
	vec3 dir = position - ubo.lightPos[lightIdx].xyz;
	dir.x *= -1;
	float dist = length(dir);
	dir = normalize(dir);
	return (dist > texture(shadow, dir).r + shadow_bias) ? 0.0f: 1.0f;
}

void main() {
	vec3 lightColor = vec3(23.47, 21.31, 20.79);
	vec3 N = (material.normalTextureSet > -1 ? getNormal() : normalize(normal));
	vec3 V = normalize(ubo.viewPos - position);

	vec3 albedo;
	float metallic;
	float roughness;
	float ao;
	vec3 emission;

	if (material.baseColorTextureSet < 0) {
		albedo = material.baseColorFactor.rgb;
	} else {
		albedo = SRGBtoLINEAR(texture(diffuseImage, texCoords0)).rgb * material.baseColorFactor.rgb;
	}

	if (material.physicalDescriptorTextureSet < 0) {
		metallic  = material.metallicFactor;
		roughness = material.roughnessFactor;
		ao		  = 1.0f;
	} else {
		vec3 metalRough = texture(metalRoughnessImage, texCoords0).rgb;
		metallic		= metalRough.b * material.metallicFactor;
		roughness		= metalRough.g * material.roughnessFactor;
		ao				= 1.0f;
	}

	if (material.occlusionTextureSet >= 0) {
		ao = texture(occlusionImage, texCoords0).r;
	}

	if (material.emissiveTextureSet < 0) {
		emission = vec3(0.0f);
	} else {
		emission = SRGBtoLINEAR(texture(emissiveImage, texCoords0)).rgb * material.emissiveColorFactor.rgb;
	}

	vec3 F0 = vec3(0.04);
	F0		= mix(F0, albedo, metallic);

	vec3 L0 = vec3(0.0f);

	for (int i = 0; i < ubo.numLights; i++) {

		vec3 L		 = normalize(ubo.lightPos[i].xyz - position);
		vec3 H		 = normalize(V + L);
		float cosine = max(dot(L, N), 0.0f);

		float dist		  = length(ubo.lightPos[i].xyz - position);
		float attenuation = 1.0 / (dist * dist);
		vec3 radiance	  = lightColor * attenuation * ubo.lightPos[i].w;
		
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
		

		float shade = calculateShadow(i);

		L0 += shade * (kd * albedo / PI + specular) * radiance * NdotL;
	}

	vec3 ambient = vec3(0.03f) * ao;
	if (debugSettings.enableIBL > 0) {
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
	}

	vec3 color	 = ambient + L0 + emission;
	outColor	 = SRGBtoLINEAR(vec4(color, 1.0f));

	if (debugSettings.viewMap > 0) {
		switch (debugSettings.viewMap) {
			case 1: {
				outColor = vec4(albedo, 1.0f);
			}; break;
			case 2: {
				outColor = vec4(0.5f + 0.5f * N, 1.0f);
			}; break;
			case 3: {
				outColor = vec4(vec3(metallic), 1.0f);
			}; break;
			case 4: {
				outColor = vec4(vec3(roughness), 1.0f);
			}; break;
			case 5: {
				outColor = vec4(vec3(ao), 1.0f);
			}; break;
			case 6: {
				outColor = vec4(emission, 1.0f);
			}; break;
			case 7: {
				outColor = vec4(position.xyz * 0.1f, 1.0f);
			}; break;
			case 8: {
				vec3 dir = position.xyz - ubo.lightPos[0].xyz;
				dir.x *= -1;
				outColor = vec4(texture(shadow, normalize(dir)).rrr * 0.1f, 1.0f);
			}; break;
		}
	}
}
