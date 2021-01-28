#version 460 core

layout (location = 0) out vec4 FS_out_frag_color;

in vec2 FS_in_tex_coord;

uniform sampler2D s_texture;

void main()
{
	FS_out_frag_color = vec4(texture(s_texture, FS_in_tex_coord).rgb, 1.f);
}