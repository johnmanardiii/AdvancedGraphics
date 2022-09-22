#version 450 core
layout(location = 0) in vec3 vertPos;
layout(location = 1) in vec2 vertTex;
layout(location = 2) in vec3 vertNor;
layout(location = 3) in ivec4 BoneIDs;
layout(location = 4) in vec4 Weights;

uniform mat4 P;
uniform mat4 V;
uniform mat4 M;

void main()
{
	gl_Position = P * V * M * vec4(vertPos, 1.0);
}
