#version 460 core

layout (location = 0) in vec3 v_pos;
layout (location = 1) in vec2 v_tex_coords;

uniform mat4 u_PV;

void main()
{
	gl_Position = u_PV * vec4(v_pos, 1.f);
}