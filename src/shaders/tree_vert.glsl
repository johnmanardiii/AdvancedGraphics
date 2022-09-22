#version 330 core
layout(location = 0) in vec3 vertPos;
layout(location = 1) in vec3 vertNor;
layout(location = 2) in vec2 vertTex;

uniform mat4 P;
uniform mat4 V;
uniform mat4 M;
uniform float glTime;

out vec3 vertex_pos;
out vec3 vertex_normal;
out vec2 vertex_tex;
out vec3 vertex_view_pos;
out vec3 vertex_view_norm;
out float ao_r;

mat4 rotateAroundY(float rot_radians)
{
	mat4 m = mat4(1.0);

	m[0] = vec4(cos(rot_radians), 0 , -sin(rot_radians), 0);
	m[2] = vec4(sin(rot_radians), 0, cos(rot_radians), 0);

	return m;
}



void main()
{
	vertex_tex = vertTex;
	vertex_normal = vec4(M * vec4(vertNor,0.0)).xyz;
	vec4 worldPos = M * vec4(vertPos, 1.0f);
	float rotationAmount = sin(glTime) * .01 * worldPos.y;
	mat4 finalM = M * rotateAroundY(rotationAmount);
	vec4 tpos =  finalM * vec4(vertPos, 1);	// model is now in world space
	vertex_pos = tpos.xyz;
	gl_Position = P * V * tpos;

	// SSAO output
	vertex_view_pos = vec3(V * tpos);
	mat3 normalMatrix = transpose(inverse(mat3(V * finalM)));
    vertex_view_norm = normalMatrix * vertNor;
	ao_r = .8;
}