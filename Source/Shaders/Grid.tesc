#version 460 core

layout (vertices = 3) out;

in vec3 TCS_in_WorldPos[];
in vec2 TCS_in_TexCoord[];

out vec3 TES_in_WorldPos[];
out vec2 TES_in_TexCoord[];

uniform vec3 u_WorldCameraPos;

float GetTessellationLevel(float dist0, float dist1)
{
	float average_dist = (dist0 + dist1) / 2.f;

    if (average_dist <= 10.f) return 20.f;
    else if (average_dist <= 20.f) return 10.f;
    else if (average_dist <= 30.f) return 5.f;
    else return 1.f;
}

void main()
{
	TES_in_WorldPos[gl_InvocationID] = TCS_in_WorldPos[gl_InvocationID];
	TES_in_TexCoord[gl_InvocationID] = TCS_in_TexCoord[gl_InvocationID];

	float dist_cam_v0 = distance(u_WorldCameraPos, TES_in_WorldPos[0]);
	float dist_cam_v1 = distance(u_WorldCameraPos, TES_in_WorldPos[1]);
	float dist_cam_v2 = distance(u_WorldCameraPos, TES_in_WorldPos[2]);

	gl_TessLevelOuter[0] = GetTessellationLevel(dist_cam_v1, dist_cam_v2);
	gl_TessLevelOuter[1] = GetTessellationLevel(dist_cam_v0, dist_cam_v2);
	gl_TessLevelOuter[2] = GetTessellationLevel(dist_cam_v0, dist_cam_v1);
	gl_TessLevelInner[0] = gl_TessLevelOuter[2];
}