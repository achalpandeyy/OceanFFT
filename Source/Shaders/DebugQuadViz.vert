#version 460 core

layout (location = 0) in vec2 VS_in_world_pos;

out vec2 FS_in_tex_coord;

void main()
{
	gl_Position = vec4(VS_in_world_pos, 0.f, 1.f);
	FS_in_tex_coord = (VS_in_world_pos + vec2(1.f, 1.f)) / 2.f;
}