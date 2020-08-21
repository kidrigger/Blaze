#version 450

layout(location = 0) in vec4 V_POSITION;
layout(location = 1, component = 0) in vec2 V_UV0;

layout(location = 0) out vec4 O_COLOR;

layout(set = 0, binding = 0) uniform sampler2D I_COLOR;

layout(push_constant) uniform PushConst {
	int vertical;
} pcb;

const float weight[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
	vec2 texelSize = 1.0f / vec2(textureSize(I_COLOR, 0));
	vec2 offset = vec2(0.0f);
	vec3 color = texture(I_COLOR, V_UV0 + offset).rgb * weight[0];
	for (int i = 1; i < 5; i++) {
		if (pcb.vertical > 0) {
			offset = vec2(0.0f, texelSize.y * i);
		} else {
			offset = vec2(texelSize.x * i, 0);
		}
		color += texture(I_COLOR, V_UV0 + offset).rgb * weight[i];
		color += texture(I_COLOR, V_UV0 - offset).rgb * weight[i];
	}

	O_COLOR = vec4(color, 1.0f);
}
