#version 460 core

layout (location = 0) out vec4 FS_out_color;

in vec3 FS_in_world_pos;
in vec3 FS_in_normal;

uniform vec3 u_world_camera_pos;

void main()
{
    vec3 normal = FS_in_normal;
	vec3 view_dir = normalize(u_world_camera_pos - FS_in_world_pos);
	vec3 reflect_dir = reflect(-view_dir, normal);

	vec3 light_dir = normalize(vec3(0.f, 0.f, -1.0));

    vec3 sea_color = vec3(0.0, 0.169668034, 0.4398943190);

    float n_dot_l = max(dot(normal, light_dir), 0.0);

    vec3 color =  n_dot_l * sea_color;

    // HDR tonemapping
    color = color / (color + vec3(1.0));

    // gamma correct
    color = pow(color, vec3(1.f / 2.2));

	FS_out_color = vec4(color, 1.0);
}