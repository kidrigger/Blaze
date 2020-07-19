#version 450

#define MAX_TEX_IN_MAT 32
#define MAX_POINT_LIGHTS 16
#define MAX_DIRECTION_LIGHTS 4
#define MAX_SHADOWS 16
#define MAX_CASCADES 4

layout(location = 0) in vec3 A_POSITION;
layout(location = 1) in vec3 A_NORMAL;
layout(location = 2) in vec2 A_UV0;
layout(location = 3) in vec2 A_UV1;

layout(location = 0) out vec4 O_POSITION;
layout(location = 1, component = 0) out vec2 O_UV0;

void main() {
	O_POSITION = vec4(A_POSITION, 1.0f);
	gl_Position = O_POSITION;
	O_UV0 = A_UV0;
}
