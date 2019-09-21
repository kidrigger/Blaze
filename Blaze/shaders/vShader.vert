#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform UniformBufferObject
{
	mat4 view;
	mat4 projection;
	vec3 lightPos;
	vec3 viewPos;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec2 inTexCoords;

layout(push_constant) uniform TRS {
	mat4 model;
} trs;

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 normals;
layout(location = 2) out vec2 texCoords;
layout(location = 6) out vec3 lightPos;
layout(location = 7) out vec3 viewPos;

layout(location = 10) out mat3 TBN;

void main() {
	gl_Position = ubo.projection * ubo.view * trs.model * vec4(inPosition, 1.0);
	outPosition = (trs.model * vec4(inPosition, 1.0)).xyz;
	normals = inNormal;
	texCoords = inTexCoords;
	lightPos = ubo.lightPos;
	viewPos = ubo.viewPos;

	vec3 T = normalize(vec3(trs.model * vec4(inTangent, 0.0)));
	vec3 B = normalize(vec3(trs.model * vec4(cross(inNormal, inTangent), 0.0)));
	vec3 N = normalize(vec3(trs.model * vec4(inNormal, 0.0)));
	TBN = mat3(T, B, N);
}
