#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoords0;
layout(location = 3) in vec2 texCoords1;
layout(location = 10) in mat3 TBN;

layout(set = 0, binding = 0) uniform UniformBufferObject
{
	mat4 view;
	mat4 projection;
	vec3 viewPos;
	vec4 lightPos[16];
} ubo;

layout(set = 1, binding = 0) uniform sampler2D diffuseImage;
layout(set = 1, binding = 1) uniform sampler2D metalRoughnessImage;
layout(set = 1, binding = 2) uniform sampler2D normalImage;
layout(set = 1, binding = 3) uniform sampler2D occlusionImage;
layout(set = 1, binding = 4) uniform sampler2D emissiveImage;

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

vec4 SRGBtoLINEAR(vec4 srgbIn)
{
	#ifdef MANUAL_SRGB
	#ifdef SRGB_FAST_APPROXIMATION
	vec3 linOut = pow(srgbIn.xyz,vec3(2.2));
	#else //SRGB_FAST_APPROXIMATION
	vec3 bLess = step(vec3(0.04045),srgbIn.xyz);
	vec3 linOut = mix( srgbIn.xyz/vec3(12.92), pow((srgbIn.xyz+vec3(0.055))/vec3(1.055),vec3(2.4)), bLess );
	#endif //SRGB_FAST_APPROXIMATION
	return vec4(linOut,srgbIn.w);;
	#else //MANUAL_SRGB
	return srgbIn;
	#endif //MANUAL_SRGB
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

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

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
	return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}

void main() {
	vec3 lightColor = vec3(23.47, 21.31, 20.79);
	vec3 N;
	if (material.normalTextureSet < 0) {
		N = normalize(normal);
	} else {
		vec3 norm = normalize(2.0f * texture(normalImage, (material.normalTextureSet == 0 ? texCoords0 : texCoords1)).rgb - 1.0f);
		N		  = normalize(TBN * norm);
	}
	vec3 V		 = normalize(ubo.viewPos - position);

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
		ao				= metalRough.r;
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
	F0      = mix(F0, albedo, metallic);

	vec3 L0 = vec3(0.0f);

	for (int i = 0; i < 1; i++) {

		vec3 L		 = normalize(ubo.lightPos[i].xyz - position);
		vec3 H		 = normalize(V + L);
		float cosine = max(dot(L, N), 0.0f);

		float dist		  = length(ubo.lightPos[i].xyz - position);
        float attenuation = 1.0 / (dist * dist);
        vec3 radiance     = lightColor * attenuation;
		
		float NDF = DistributionGGX(N, H, roughness);
		float G   = GeometrySmith(N, V, L, roughness);
		vec3 F  = fresnelSchlick(max(dot(H, V), 0.0f), F0);

		vec3 ks = F;
		vec3 kd = vec3(1.0f) - ks;
		kd *= 1.0f - metallic;

		vec3 numerator    = NDF * G * F;
		float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
		vec3 specular     = numerator / max(denominator, 0.001);

		float NdotL = max(dot(N, L), 0.0f);
		L0 += (kd * albedo / PI + specular) * radiance * NdotL;
	}

	vec3 ambient = vec3(0.03f) * albedo * ao;
	vec3 color	 = ambient + L0 + emission;
	outColor = SRGBtoLINEAR(vec4(color, 1.0f));
}
