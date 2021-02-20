#version 460 core

layout (location = 0) in vec3 VS_in_world_pos;
layout (location = 1) in vec2 VS_in_tex_coord;

out vec3 FS_in_world_pos;
out vec2 FS_in_tex_coord;

uniform mat4 u_pv;

uniform sampler2D u_displacement_map;

void main()
{
	vec3 position = VS_in_world_pos + texture(u_displacement_map, VS_in_tex_coord).rgb;
	gl_Position = u_pv * vec4(position, 1.f);

	FS_in_world_pos = position;
	FS_in_tex_coord = VS_in_tex_coord;
}