#version 460 core

layout (location = 0) in vec3 VS_in_WorldPos;
layout (location = 1) in vec2 VS_in_TexCoord;

out vec3 TCS_in_WorldPos;
out vec2 TCS_in_TexCoord;

void main()
{
	TCS_in_WorldPos = VS_in_WorldPos;
	TCS_in_TexCoord = VS_in_TexCoord;
}