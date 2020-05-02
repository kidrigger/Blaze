#version 450

layout(location = 0) in vec3 A_POSITION;
layout(location = 1) in vec3 A_NORMAL;
layout(location = 2) in vec2 A_UV0;
layout(location = 3) in vec2 A_UV1;

layout(location = 0) out vec3 outPosition;

void main() {
	gl_Position = vec4(A_POSITION, 1.0f);
	outPosition = A_POSITION;
}
