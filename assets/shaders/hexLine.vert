#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

const int perRow = 128;
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

	gl_Position = vec4(center + radius * offset, 0.0, 1.0);
	gl_Position.x /= ratio;
	// gl_Position.xy *= 1;
}