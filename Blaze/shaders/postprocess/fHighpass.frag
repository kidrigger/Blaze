#version 450

layout(location = 0) in vec4 V_POSITION;
layout(location = 1, component = 0) in vec2 V_UV0;

layout(location = 0) out vec4 O_COLOR;

layout(set = 0, binding = 0) uniform sampler2D I_COLOR;

layout(push_constant) uniform LightIdx {
	float threshold;
	float tolerance;
} pcb;

void main() {
	vec3 color = texture(I_COLOR, V_UV0).rgb;
	float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
	O_COLOR = mix(vec4(0,0,0,1), vec4(color, 1.0f), smoothstep(0.0f, pcb.tolerance, brightness - pcb.threshold));
}
