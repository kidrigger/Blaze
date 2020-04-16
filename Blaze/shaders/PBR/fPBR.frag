#version 450

#define MAX_POINT_LIGHTS 16
#define MAX_DIR_LIGHTS 1
#define MAX_CSM_SPLITS 4
#define MAX_STATIC_POINT_LIGHTS 4
#define MAX_STATIC_DIR_LIGHTS 4

#define MANUAL_SRGB 1

const float shadow_bias = 0.05f;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 viewPosition;
layout(location = 3) in vec2 texCoords0;
layout(location = 4) in vec2 texCoords1;
layout(location = 5) in vec4 lightCoord[MAX_DIR_LIGHTS][MAX_CSM_SPLITS];

layout(set = 0, binding = 0) uniform UpdateOnDemand {
	mat4 projection;										// Seldom
    vec4 csmSplits[MAX_DIR_LIGHTS];							// Seldom
	ivec4 shadowIdx[MAX_POINT_LIGHTS/4];					// Seldom
	vec4 lightDir[MAX_STATIC_DIR_LIGHTS];					// Seldom
	mat4 lightDirPV[MAX_STATIC_DIR_LIGHTS][MAX_CSM_SPLITS];	// Seldom
	int numLights;											// Seldom
	int numDirLights;										// Seldom
} onDemand;

layout(set = 0, binding = 1) uniform PerFrame {
	mat4 view;
	vec4 viewPos;
	mat4 lightDir[MAX_DIR_LIGHTS];
	mat4 lightDirPV[MAX_DIR_LIGHTS][MAX_CSM_SPLITS];
	vec4 lightPos[MAX_POINT_LIGHTS];
} perFrame;

layout(set = 0, binding = 1) uniform DebugSettings {
	int viewMap;
	int enableSkybox;
	int enableIBL;
	float exposure;
	float gamma;
} debugSettings;

layout(set = 1, binding = 0) uniform sampler2DArray diffuseImage;
layout(set = 1, binding = 1) uniform sampler2DArray metalRoughnessImage;
layout(set = 1, binding = 2) uniform sampler2DArray normalImage;
layout(set = 1, binding = 3) uniform sampler2DArray occlusionImage;
layout(set = 1, binding = 4) uniform sampler2DArray emissiveImage;

layout(set = 2, binding = 0) uniform samplerCube skybox;
layout(set = 2, binding = 1) uniform samplerCube irradianceMap;
layout(set = 2, binding = 2) uniform samplerCube prefilteredMap;
layout(set = 2, binding = 3) uniform sampler2D brdfLUT;

layout(set = 3, binding = 0) uniform samplerCube shadow[MAX_POINT_LIGHTS];
layout(set = 3, binding = 1) uniform sampler2DArray dirShadow[MAX_DIR_LIGHTS];

layout(location = 0) out vec4 outColor;

const float PI = 3.1415926535897932384626433832795f;

void main() {

}
