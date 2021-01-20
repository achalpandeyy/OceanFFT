#version 460 core

layout (location = 0) in vec2 v_pos;

out vec2 tex_coords;

void main()
{
	gl_Position = vec4(v_pos, 0.f, 1.f);
	tex_coords = (v_pos + vec2(1.f, 1.f)) / 2.f;
}