#version 450 core 

out vec4 color;
in vec2 fragTex;

uniform int bloomEnabled;
uniform mat4 P;
uniform mat4 Vcam;

uniform sampler2D colortex;
uniform sampler2D brighttex;
uniform usampler2D lightmaskval;
uniform sampler2D viewPosTex;
uniform sampler2D viewNormTex;

const int raysteps = 30;
const int binsteps = 5;

vec3 BinarySearch(inout vec3 dir, inout vec3 pos)
{
    float depth;
    vec4 sspos;
    for(int i = 0; i < binsteps; i++)
    {
        sspos = P * vec4(pos, 1.0);
        sspos.xy /= sspos.w;
        sspos.xy = sspos.xy * .5 + .5;
        vec2 coords = sspos.xy;
        vec4 viewpos = texture(viewPosTex, coords);
        float ddepth = pos.z - viewpos.z;
        dir *= .5;
        if(ddepth > 0)
            pos += dir;
        else
            pos -= dir;
    }
    sspos = P * vec4(pos, 1.0);
    sspos.xy /= sspos.w;
    sspos.xy = sspos.xy * .5 + .5;
    return vec3(sspos.xy, depth);
}


vec4 raymarch(vec3 dir, vec3 pos)
{
    dir *= 1;
    vec4 sspos = P * vec4(pos, 1);
    for(int i = 0; i < raysteps; i++)
    {
        pos += dir;

        sspos = P * vec4(pos, 1);
        sspos.xy /= sspos.w;
        sspos.xy = sspos.xy * .5 + .5;
        vec4 vpos = vec4(texture(viewPosTex, sspos.xy).xyz, 1);
        float ddepth = pos.z - vpos.z;
        if(ddepth <= 0)
        {
            //return vec4(BinarySearch(dir, pos), 0);
            return vec4(sspos.xy, ddepth, 0);
        }
    }
    return vec4(sspos.xy, 0, 0);
}

// ACES tonemapping curve from: https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 ACESFilm(vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

void main()
{
    if(bloomEnabled == 1)
    {
        color.rgb = vec3(texture(colortex, fragTex).rgb) + 
                    vec3(texture(brighttex, fragTex).rgb) * .9;
    }
    else
    {
        color.rgb = vec3(texture(colortex, fragTex).rgb);
    }

    // reflection code:
    uint lightmask = texture(lightmaskval, fragTex).r;
    if(lightmask == 2)
    {
        vec3 vpos = texture(viewPosTex, fragTex).rgb;
        vec3 vnorm = normalize(texture(viewNormTex, fragTex).rgb);
        vec3 reflected = normalize(reflect(normalize(vpos.xyz), vnorm.xyz));
        vec4 coords = raymarch(reflected, vpos);
        vec3 SSR = texture(colortex, coords.xy, 3).rgb;
        color.rgb += SSR * .15;
    }
    color.rgb = ACESFilm(color.rgb);
    color.a = 1.0;
}
