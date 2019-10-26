#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform UniformBufferObject
{
	mat4 view;
	mat4 projection;
	vec3 viewPos;
	vec3 lightPos[16];
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoords0;
layout(location = 3) in vec2 inTexCoords1;

layout(push_constant) uniform TRS {
	mat4 model;
} trs;

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 normals;
layout(location = 2) out vec2 texCoords0;
layout(location = 3) out vec2 texCoords1;

layout(location = 4) out vec3 color;

void main() {
	vec4 pos = ubo.projection * ubo.view * vec4(inPosition * 0.1f + ubo.viewPos, 1.0);
	gl_Position = pos.xyww;
	outPosition = inPosition;
	outPosition.x *= -1.0f;
}