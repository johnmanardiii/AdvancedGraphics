#version 330 core
in vec3 vertex_normal;
in vec3 vertex_pos;
in vec2 vertex_tex;

uniform sampler2D tex;
uniform vec3 camPos;

layout(location = 0) out vec4 color;
layout(location = 1) out vec4 pos;
layout(location = 2) out vec4 normal;
layout(location = 3) out uint lightmask;

void main()
{
	vec4 tcol = texture(tex, vertex_tex);
	color = tcol;
	color.rgb *= 0.7;
	lightmask = 1U;
}



