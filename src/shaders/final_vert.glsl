#version 450 core
layout(location = 0) in vec4 vertPos;
layout(location = 2) in vec2 vertTex;

out vec2 fragTex;

void main()
{
	gl_Position = vertPos;
	fragTex = vertTex;
}
