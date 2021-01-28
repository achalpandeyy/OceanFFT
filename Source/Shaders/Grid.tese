#version 460 core

layout (triangles, equal_spacing, ccw) in;

in vec3 TES_in_world_pos[];
in vec2 TES_in_tex_coord[];

out vec3 FS_in_world_pos;
out vec3 FS_in_normal;

uniform mat4 u_pv;
uniform sampler2D s_displacement_y;
uniform sampler2D s_displacement_x;
uniform sampler2D s_displacement_z;
uniform sampler2D s_normal_map;

uniform float u_displacement_scale;
uniform float u_choppiness;

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
	FS_in_world_pos = Interpolate3D(TES_in_world_pos[0], TES_in_world_pos[1], TES_in_world_pos[2]);

    vec2 tex_coord = Interpolate2D(TES_in_tex_coord[0], TES_in_tex_coord[1], TES_in_tex_coord[2]);

	FS_in_world_pos.y += texture(s_displacement_y, tex_coord).r * u_displacement_scale;
	FS_in_world_pos.x -= texture(s_displacement_x, tex_coord).r * u_choppiness;
	FS_in_world_pos.z -= texture(s_displacement_z, tex_coord).r * u_choppiness;

	FS_in_normal = texture(s_normal_map, tex_coord).xyz;

	gl_Position = u_pv * vec4(FS_in_world_pos, 1.0);
}