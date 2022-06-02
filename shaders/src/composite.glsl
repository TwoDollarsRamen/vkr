#version 450

#begin VERTEX

layout (location = 0) in vec2 position;
layout (location = 1) in vec2 uv;

layout (location = 0) out VertexOut {
	vec2 uv;
} vs_out;

void main() {
	vs_out.uv = uv;

	gl_Position = vec4(position, 0.0, 1.0);
}

#end VERTEX

#begin FRAGMENT

#include "pp_common.glsl"
#include "pp_config.glsl"

layout (set = 1, binding = 0) uniform sampler2D tonemapped_scene;
layout (set = 1, binding = 1) uniform sampler2D bloom;

void main() {
	color = texture(tonemapped_scene, fs_in.uv) + texture(bloom, fs_in.uv) * config.bloom_intensity;
}

#end FRAGMENT
