#version  450 core
out vec4 FragColor;

in vec2 fragTex;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D texNoise;
uniform sampler2D ao_radius;

#define NUM_SAMPLES 64
#define NUM_HBAO_SAMPLES 16
#define NUM_RAYMARCH 16
#define PI 3.1415
layout (std140, binding = 0) uniform Samples
{
    vec4 samples[NUM_SAMPLES];
    vec4 HbaoSamples[NUM_HBAO_SAMPLES];
};


int kernelSize = NUM_SAMPLES;
uniform float radius;       // not used atm, can be used for debuggin 
uniform float bias;
uniform vec2 noiseScale;
uniform mat4 projection;
uniform int is_ssao;

// HBAO Data:
float strengthPerRay = 0.1875;	// strength / numRays
float halfSampleRadius = .25;	// sampleRadius / 2
float fallOff = 2.0;			// the maximum distance to count samples
float hbao_bias = .03;				// minimum factor to start counting occluders
float input_radius = 50.0;
float ssao_bias = .1;

// Given a samplePos (in view space) converts it to a texture coord using Projection matrix.
vec2 view_to_tex(vec3 samplePos)
{
    // project sample position (to sample texture) (to get position on screen/texture)
    vec4 offset = vec4(samplePos, 1.0);
    offset = projection * offset; // from view to clip-space
    offset.xy /= offset.w; // perspective divide
    return offset.xy * 0.5 + 0.5; // transform to range 0.0 - 1.0
}

vec4 get_ssao()
{
    // get input for SSAO algorithm
    vec3 fragPos = texture(gPosition, fragTex).xyz;
    vec3 normal = normalize(texture(gNormal, fragTex).rgb);
    vec3 randomVec = normalize(texture(texNoise, fragTex * noiseScale).xyz);
    // create TBN change-of-basis matrix: from tangent-space to view-space
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);
    // iterate over the sample kernel and calculate occlusion factor
    float occlusion = 0.0;
    for(int i = 0; i < kernelSize; ++i)
    {
        // get sample position
        vec3 samplePos = TBN * samples[i].xyz; // from tangent to view-space
        samplePos = fragPos + samplePos * input_radius;
        
        vec2 tex_coords = view_to_tex(samplePos);
        // get sample depth
        float sampleDepth = texture(gPosition, tex_coords).z; // get depth value of kernel sample
        
        // range check & accumulate
        float rangeCheck = smoothstep(0.0, 1.0, input_radius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + ssao_bias ? 1.0 : 0.0) * rangeCheck;           
    }
    occlusion = 1.0 - (occlusion / kernelSize);
    return vec4(vec3(occlusion), 1);
}

float raymarch_hbao_original(vec3 raymarch_dir, vec3 viewPos)
{
    float raymarch_dist = input_radius / float(NUM_RAYMARCH);
    float largest_angle = 0.0;
    float occlusion = 0.0;
    for(int i = 0; i < NUM_RAYMARCH; i++)
    {
        vec3 samplePos = viewPos + raymarch_dir * raymarch_dist * float(i);
        vec2 tex_coords = view_to_tex(samplePos);
        float sampleDepth = texture(gPosition, tex_coords).z; // get depth value of raymarch step
        if(sampleDepth < samplePos.z + bias)
        {
            continue;
        }
        float angle = atan(abs(viewPos.z - sampleDepth) / length(raymarch_dist * i));
        if(largest_angle > angle)
        {
            continue;
        }
        // let pi / 2 = 1 (full occluded)
        largest_angle = angle;
        // contriube to occlusion:
        float contribution = clamp((angle / (PI / 4.0)) / (i * .95), 0.0, 1.0);
        float rangeCheck = smoothstep(0.0, 1.0, input_radius / abs(viewPos.z - sampleDepth));
        occlusion += contribution * rangeCheck;
    }
    return occlusion;
}

float raymarch_hbao(vec3 raymarch_dir, vec3 viewPos)
{
    vec3 original_ray_dir = raymarch_dir;
    float raymarch_fact = 1.0 / float(NUM_RAYMARCH);
    float raymarch_dist = input_radius * raymarch_fact;
    for(int i = 0; i < NUM_RAYMARCH; i++)
    {
        vec3 samplePos = viewPos + raymarch_dir * raymarch_dist * float(i);
        vec2 tex_coords = view_to_tex(samplePos);
        vec3 tex_pos = texture(gPosition, tex_coords).rgb;
        float sampleDepth = tex_pos.z; // get depth value of raymarch step
        if(sampleDepth < samplePos.z + bias || abs(sampleDepth - samplePos.z) > radius)
        {
            continue;
        }
        raymarch_dir = normalize(tex_pos - viewPos) * raymarch_fact;
    }
    return (dot(normalize(raymarch_dir), normalize(original_ray_dir)));
}

float raymarch_hbao_snap(vec3 raymarch_dir, vec3 viewPos)
{
    vec3 original_ray_dir = raymarch_dir;
    float raymarch_fact = 1.0 / float(NUM_RAYMARCH);
    float raymarch_dist = input_radius * raymarch_fact;
    for(int i = 0; i < NUM_RAYMARCH; i++)
    {
        raymarch_dir += normalize(raymarch_dir) * raymarch_dist;
        if(length(raymarch_dir) > radius)
        {
            break;
        }
        vec3 samplePos = viewPos + raymarch_dir * raymarch_dist * float(i);
        vec2 tex_coords = view_to_tex(samplePos);
        vec3 tex_pos = texture(gPosition, tex_coords).rgb;
        float sampleDepth = tex_pos.z; // get depth value of raymarch step
        if(sampleDepth < samplePos.z + bias || abs(sampleDepth - samplePos.z) > radius
            || length(tex_pos - viewPos) > radius)
        {
            continue;
        }
        raymarch_dir = tex_pos - viewPos;
    }
    return (dot(normalize(raymarch_dir), normalize(original_ray_dir)));
}

vec4 get_hbao()
{
    vec3 fragPos = texture(gPosition, fragTex).xyz;
    vec3 normal = normalize(texture(gNormal, fragTex).rgb);
    vec3 randomVec = normalize(texture(texNoise, fragTex * noiseScale).xyz);
    // create TBN change-of-basis matrix: from tangent-space to view-space
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);
    // calculate occlusion factor
    float occlusion = 0.0;
    
    for(int i = 0; i < NUM_HBAO_SAMPLES; i++)
    {
        vec3 raymarch_dir = normalize(TBN * HbaoSamples[i].xyz);
        occlusion += raymarch_hbao_snap(raymarch_dir, fragPos);
    }
    occlusion = (occlusion / NUM_HBAO_SAMPLES);
    occlusion = pow(occlusion, 1.2);
    occlusion = clamp(occlusion, 0, 1);
    return vec4(vec3(occlusion), 1);
}


void main()
{
    //input_radius = texture(ao_radius, fragTex).r;
    input_radius = radius;

    // FragColor = get_ssao(ao_radius);
    if(is_ssao == 0)
    {
        FragColor = get_hbao();
    }
    else
    {
        input_radius = .6;
        FragColor = get_ssao();
    }
    //FragColor = vec4(1, 0, 0, 1);
    //FragColor = vec4(HbaoSamples[1].xy, 0, 1);
}
