#version 460 core

layout (location = 0) in vec3 VS_in_world_pos;
layout (location = 1) in vec2 VS_in_tex_coord;

// out vec3 FS_in_pos_world;
// out vec3 FS_in_normal;
out vec2 FS_in_tex_coord;

uniform mat4 u_pv;

// uniform sampler2D u_displacement_y;
// uniform sampler2D u_displacement_x;
// uniform sampler2D u_displacement_z;
// uniform sampler2D u_normal_map;

// uniform float u_displacement_scale;
// uniform float u_choppiness;

void main()
{
	gl_Position = u_pv * vec4(VS_in_world_pos, 1.f);
	FS_in_tex_coord = VS_in_tex_coord;

	// FS_in_pos_world.y += texture(u_displacement_y, VS_in_tex_coord).r * u_displacement_scale;
	// FS_in_pos_world.x -= texture(u_displacement_x, VS_in_tex_coord).r * u_choppiness;
	// FS_in_pos_world.z -= texture(u_displacement_z, VS_in_tex_coord).r * u_choppiness;
	// 
	// FS_in_normal = texture(u_normal_map, VS_in_tex_coord).xyz;
}