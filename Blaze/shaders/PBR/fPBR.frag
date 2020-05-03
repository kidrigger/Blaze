#version 450

const float shadow_bias = 0.05f;

layout(location = 0) in vec4 position;

layout(location = 0) out vec4 outColor;

void main()
{
	outColor = vec4(position.xyz, 1.0f);
}
