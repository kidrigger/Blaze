#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D equirectangularMap;

layout(push_constant) uniform PushConsts {
	layout (offset = 64) float deltaPhi;
	layout (offset = 68) float deltaTheta;
} consts;

const float PI = 3.1415926535897932384626433832795f;

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x),asin(v.y));
    uv *= invAtan;
    uv = 0.5f - uv;
    return uv;
}

void main() {
	vec2 uv = SampleSphericalMap(normalize(inPosition));
	vec3 color = texture(equirectangularMap, uv).rgb;

	outColor =  vec4(color, 1.0f);
}
