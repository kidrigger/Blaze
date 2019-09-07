#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoords;
layout(location = 6) in vec3 lightPos;
layout(location = 7) in vec3 viewPos;
layout(location = 10) in mat3 TBN;

layout(set = 1, binding = 0) uniform sampler2D diffuseImage;
layout(set = 1, binding = 1) uniform sampler2D metalRoughnessImage;
layout(set = 1, binding = 2) uniform sampler2D normalImage;
layout(set = 1, binding = 3) uniform sampler2D occlusionImage;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359f;

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
	vec3 norm = normalize(2.0f * texture(normalImage, texCoords).rgb - 1.0f);
	vec3 N		 = normalize(TBN * norm);
	vec3 V		 = normalize(viewPos - position);

	vec3 albedo		= texture(diffuseImage, texCoords).rgb * 0.5f;
	vec3 metalRough = texture(metalRoughnessImage, texCoords).rgb;
	float metallic	= metalRough.b;
	float roughness = metalRough.g;
	float ao		= texture(occlusionImage, texCoords).r;

	vec3 F0 = vec3(0.04); 
	F0      = mix(F0, albedo, metallic);

	vec3 L0 = vec3(0.0f);
	{
		vec3 L		 = normalize(lightPos - position);
		vec3 H		 = normalize(V + L);
		float cosine = max(dot(L, N), 0.0f);

		float dist		  = length(lightPos - position);
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
	vec3 color	 = ambient + L0;
	color		 = color / (color + vec3(1.0));
	color		 = pow(color, vec3(1.0/2.2));
	outColor	 = vec4(color, 1.0f);
}
