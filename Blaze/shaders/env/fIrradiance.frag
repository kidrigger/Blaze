#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform samplerCube skybox;
layout(set = 1, binding = 1) uniform samplerCube irradianceMap;
layout(set = 1, binding = 2) uniform samplerCube prefilteredMap;
layout(set = 1, binding = 3) uniform sampler2D brdfLUT;

layout(push_constant) uniform PushConsts {
	mat4 mvp;
	float deltaPhi;
	float deltaTheta;
} consts;

const float PI = 3.1415926535897932384626433832795f;

void main() {

	vec3 N = normalize(inPosition);
	vec3 up = vec3(0.0f, 1.0f, 0.0f);
	vec3 right = normalize(cross(up, N));
	up = cross(right, N);

	const float TWO_PI = PI * 2.0f;
	const float HALF_PI = PI * 0.5f;

	vec3 color = vec3(0.0f);
	uint sampleCount = 0u;
	for (float phi = 0.0f; phi < TWO_PI; phi += consts.deltaPhi) {
		for (float theta = 0.0f; theta < HALF_PI; theta += consts.deltaTheta) {
			vec3 radial = sin(phi) * up + cos(phi) * right;
			vec3 sampleDir = cos(theta) * N + sin(theta) * radial;
			color += texture(skybox, sampleDir).rgb * cos(theta) * sin(theta);
			sampleCount++;
		}
	}
	outColor =  vec4(PI * color / float(sampleCount), 1.0f);
}
