#version 460 core

layout (location = 0) out vec4 FragColor;

in vec2 tex_coords;

uniform sampler2D s_Texture;

void main()
{
	FragColor = vec4(texture(s_Texture, tex_coords).rrr, 1.f);
	// FragColor = texture(s_Texture, tex_coords);
}