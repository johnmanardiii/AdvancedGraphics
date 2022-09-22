#version 450 
#extension GL_ARB_shader_storage_buffer_object : require
layout(local_size_x = 1, local_size_y = 1) in;


layout(rgba16, binding = 0) uniform image2D img_input;	// input image

layout (std430, binding=0) volatile buffer shader_data
{ 
	int maxLum;
	int minLum;	// probs gotta reset these my boi
	int avgLum;
};

#define FFACT 100000

float luminance(vec3 v)
{
    return dot(v, vec3(0.2126f, 0.7152f, 0.0722f));
}

void main() 
{
	ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);
	float luminance = luminance(imageLoad(img_input, pixel_coords).rgb);
	int iluminance = int(luminance * FFACT);
	// atomicAdd(avgLum, iluminance);

	atomicMax(maxLum, iluminance);
	//if(luminance > 0)
		//atomicMin(minLum, iluminance);
}