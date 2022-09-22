#version 330 core
layout(location = 0) in vec3 vertPos;
layout(location = 1) in vec3 vertNor;
layout(location = 2) in vec2 vertTex;

uniform mat4 P;
uniform mat4 V;
uniform mat4 M;
uniform float glTime;


mat4 rotateAroundY(float rot_radians)
{
	mat4 m = mat4(1.0);

	m[0] = vec4(cos(rot_radians), 0 , -sin(rot_radians), 0);
	m[2] = vec4(sin(rot_radians), 0, cos(rot_radians), 0);

	return m;
}



void main()
{
	vec4 worldPos = M * vec4(vertPos, 1.0f);
	float rotationAmount = sin(glTime) * .01 * worldPos.y;
	vec4 rotatedPos = rotateAroundY(rotationAmount) * vec4(vertPos, 1);
	vec4 tpos =  M * rotatedPos;	// model is now in world space
	gl_Position = P * V * tpos;
}