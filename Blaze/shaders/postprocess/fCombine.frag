#version 450

layout(location = 0) in vec4 V_POSITION;
layout(location = 1, component = 0) in vec2 V_UV0;

layout(location = 0) out vec4 O_COLOR;

layout(set = 0, binding = 0) uniform sampler2D I_COLOR;

layout(push_constant) uniform LightIdx {
	float threshold;
} pcb;

void main() {
	O_COLOR = vec4(texture(I_COLOR, V_UV0).rgb, 1.0f);
}
