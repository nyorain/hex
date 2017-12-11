#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec2 color;
layout(location = 0) out vec2 uv;
layout(location = 1) out vec4 col;

const int perRow = 256;
const float cospi6 = 0.866025;
const float radius = (1.f / perRow) / cospi6;
const float ratio = 1920.f / 1080.f;

// the vertex values to use
// they form a hexagon to be drawn
const vec2[] values = {
	{cospi6, .5f},
	{0.f, 1.f},
	{-cospi6, .5f},
	{-cospi6, -.5f},
	{0.f, -1.f},
	{cospi6, -.5f},
};

float rand(vec2 co) {
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main() {
	int cx = gl_InstanceIndex % perRow;
	int cy = gl_InstanceIndex / perRow;

	vec2 center;
	center.x = 2 * radius * cospi6 * cx - 1.f;
	center.y = 1.414 * radius * cy - 1.f;

	// TODO: we currently miss the last hexagon here...
	if(cy % 2 == 1) {
		center.x += cospi6 * radius;
	}

	vec2 offset = values[gl_VertexIndex % 6];

	uv = offset;
	col = color;

	gl_Position = vec4(center + radius * offset, 0.0, 1.0);
	gl_Position.x /= ratio;
	// gl_Position.xy *= 1;
}