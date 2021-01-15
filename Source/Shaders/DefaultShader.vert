#version 460 core

layout (location = 0) in vec3 v_pos;
layout (location = 1) in vec2 v_tex_coords;

void main()
{
	gl_Position = vec4(v_pos.x, v_pos.z, 0.f, 1.f);
}