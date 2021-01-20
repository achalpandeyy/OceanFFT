#version 460 core

layout (location = 0) out vec4 FragColor;

in vec2 tex_coords;

uniform sampler2D s_Texture;

void main()
{
	FragColor = texture(s_Texture, tex_coords);
}