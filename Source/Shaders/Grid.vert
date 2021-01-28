#version 460 core

layout (location = 0) in vec3 VS_in_world_pos;
layout (location = 1) in vec2 VS_in_tex_coord;

out vec3 TCS_in_world_pos;
out vec2 TCS_in_tex_coord;

void main()
{
	TCS_in_world_pos = VS_in_world_pos;
	TCS_in_tex_coord = VS_in_tex_coord;
}