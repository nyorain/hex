#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec2 uv;
layout(location = 1) in vec4 color;
layout(location = 0) out vec4 fragColor;

void main() {
	// fragColor = vec4(uv, 0.5, 1);
	fragColor = color;
}