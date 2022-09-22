#version 330 core
in vec3 vertex_normal;
in vec3 vertex_pos;
in vec2 vertex_tex;
in vec3 vertex_view_pos;
in vec3 vertex_view_norm;
in float ao_r;

uniform vec3 camPos;
uniform sampler2D tex;

layout(location = 0) out vec4 color;
layout(location = 1) out vec4 pos;
layout(location = 2) out vec4 normal;
layout(location = 3) out int lightmask;
layout(location = 4) out vec4 view_pos;
layout(location = 5) out vec4 view_norm;
layout(location = 6) out vec4 ao_radius;

void main()
{
   color.rgb = texture(tex, vertex_tex).rgb;
   color.a=1;
   pos = vec4(vertex_pos, 1.0);
   normal = vec4(vertex_normal, 1.0);
   lightmask = 0;

   // MAKE CRYSTALS SEPERATE THIS IS INSANE BEHAVIOR TO MAKE EVERYTHING RED REFLECTIVE
   if(dot(vec3(1, -1, -1), color.rgb) > .3)
   {
		lightmask = 2;
   }
   view_pos = vec4(vertex_view_pos, 1.0);
   view_norm = vec4(vertex_view_norm, 1.0);
   ao_radius = vec4(ao_r, 0, 0, 1);
}

