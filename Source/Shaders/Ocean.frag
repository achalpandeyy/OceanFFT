#version 460 core

layout (location = 0) out vec4 FS_out_color;

// in vec3 FS_in_pos_world;
// in vec3 FS_in_normal;
in vec2 FS_in_tex_coord;

// uniform vec3 u_world_camera_pos;
uniform sampler2D u_input;

// vec3 HDR(vec3 color, float exposure)
// {
//     return 1.0 - exp(-color * exposure);
// }

void main()
{
    // vec3 normal = FS_in_normal;
    // 
	// vec3 view_dir = normalize(u_world_camera_pos - FS_in_pos_world);
    // float fresnel = 0.02 + 0.98 * pow(1.0 - dot(normal, view_dir), 5.0);
    // 
    // vec3 sun_direction = vec3(-1.0, 1.0, 1.0);
    // vec3 sky_color = vec3(3.2, 9.6, 12.8);
    // vec3 ocean_color = vec3(0.004, 0.016, 0.047);
    // float exposure = 0.35;
    // 
    // vec3 sky = fresnel * sky_color;
    // float diffuse = clamp(dot(normal, normalize(sun_direction)), 0.0, 1.0);
    // vec3 water = (1.0 - fresnel) * ocean_color * sky_color * diffuse;
    // 
    // vec3 color = sky + water;

    // FS_out_color = vec4(HDR(color, exposure), 1.0);

    // vec4 test_color = 10000.0 * texture(u_input, FS_in_tex_coord);
    vec4 test_color = texture(u_input, FS_in_tex_coord);
    FS_out_color = vec4(test_color.r, test_color.g, test_color.b, 1.0);
}