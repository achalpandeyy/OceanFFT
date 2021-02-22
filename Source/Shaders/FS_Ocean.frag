#version 460 core

layout (location = 0) out vec4 FS_out_color;

in vec3 FS_in_world_pos;
in vec2 FS_in_tex_coord;

uniform vec3 u_world_camera_pos;
uniform vec3 u_sun_direction;
uniform int u_wireframe;

uniform sampler2D u_normal_map;

vec3 HDR(vec3 color, float exposure)
{
    return 1.0 - exp(-color * exposure);
}

void main()
{
    if (u_wireframe)
    {
        FS_out_color = vec4(0.f, 0.f, 0.f, 1.f);
        return;
    }

    vec3 normal = texture(u_normal_map, FS_in_tex_coord).xyz;
    
	vec3 view_dir = normalize(u_world_camera_pos - FS_in_world_pos);
    float fresnel = 0.02f + 0.98f * pow(1.f - dot(normal, view_dir), 5.f);
    
    vec3 sky_color = vec3(3.2f, 9.6f, 12.8f);
    vec3 ocean_color = vec3(0.004f, 0.016f, 0.047f);
    float exposure = 0.35f;
    
    vec3 sky = fresnel * sky_color;
    float diffuse = clamp(dot(normal, normalize(-u_sun_direction)), 0.f, 1.f);
    vec3 water = (1.f - fresnel) * ocean_color * sky_color * diffuse;
    
    vec3 color = sky + water;

    FS_out_color = vec4(HDR(color, exposure), 1.f);
}