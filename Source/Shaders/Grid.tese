#version 460 core

layout (triangles, equal_spacing, ccw) in;

in vec3 TES_in_WorldPos[];
in vec2 TES_in_TexCoord[];

uniform mat4 u_PV;
uniform sampler2D s_DisplacementY;
uniform sampler2D s_DisplacementX;
uniform sampler2D s_DisplacementZ;

vec2 Interpolate2D(vec2 v0, vec2 v1, vec2 v2)
{
    return vec2(gl_TessCoord.x) * v0 + vec2(gl_TessCoord.y) * v1 + vec2(gl_TessCoord.z) * v2;
}

vec3 Interpolate3D(vec3 v0, vec3 v1, vec3 v2)
{
    return vec3(gl_TessCoord.x) * v0 + vec3(gl_TessCoord.y) * v1 + vec3(gl_TessCoord.z) * v2;
}

void main()
{
	float u_DisplacementScale = 0.5f;
	float u_Choppiness = 0.75f;

	vec3 world_pos = Interpolate3D(TES_in_WorldPos[0], TES_in_WorldPos[1], TES_in_WorldPos[2]);
    vec2 tex_coord = Interpolate2D(TES_in_TexCoord[0], TES_in_TexCoord[1], TES_in_TexCoord[2]);

	world_pos.y += texture(s_DisplacementY, tex_coord).r * u_DisplacementScale;
	world_pos.x -= texture(s_DisplacementX, tex_coord).r * u_Choppiness;
	world_pos.z -= texture(s_DisplacementZ, tex_coord).r * u_Choppiness;

	gl_Position = u_PV * vec4(world_pos, 1.f);
}