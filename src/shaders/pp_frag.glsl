#version 450 core 

layout (location = 0) out vec4 color;
layout (location = 1) out vec4 bright;
layout (location = 2) out vec4 no_tonemapping;

in vec2 fragTex;

uniform vec3 camPos;
uniform vec3 lightpos;
uniform vec3 lightdir;
uniform mat4 lightSpace;
uniform int shadowsEnabled;
uniform int tonemappingEnabled;
uniform vec2 maxmin;

uniform sampler2D colortex;
uniform sampler2D postex;
uniform sampler2D normtex;
uniform usampler2D lightmask;
uniform sampler2D shadowMapTex;
uniform sampler2D ssao;

const float max_light = 1;

float luminance(vec3 v)
{
    return dot(v, vec3(0.2126f, 0.7152f, 0.0722f));
}

// Evaluates how shadowed a point is using PCF with 5 samples
// Credit: Sam Freed - https://github.com/sfreed141/vct/blob/master/shaders/phong.frag
float calcShadowFactor(vec4 lightSpacePosition) {
    vec3 shifted = (lightSpacePosition.xyz / lightSpacePosition.w + 1.0) * 0.5;

    float shadowFactor = 0;
    float bias = 0.0001;
    float fragDepth = shifted.z - bias;

    if (fragDepth > 1.0) {
        return 0.0;
    }

    const int numSamples = 5;
    const ivec2 offsets[numSamples] = ivec2[](
        ivec2(0, 0), ivec2(1, 0), ivec2(0, 1), ivec2(-1, 0), ivec2(0, -1)
    );

    for (int i = 0; i < numSamples; i++) {
        if (fragDepth > textureOffset(shadowMapTex, shifted.xy, offsets[i]).r) {
            shadowFactor += 1;
        }
    }
    shadowFactor /= numSamples;

    return shadowFactor;
}

void getDefaultLighting()
{
	vec3 texturecolor = texture(colortex, fragTex).rgb;
	vec3 vertex_normal = normalize(texture(normtex, fragTex).rgb);
	vec3 vertex_pos = texture(postex, fragTex).rgb;

    vec4 fragLightSpacePos = lightSpace * vec4(vertex_pos, 1.0);
	float shadowFactor;
	if(shadowsEnabled == 1)
	{
		shadowFactor = 1.0 - calcShadowFactor(fragLightSpacePos);
	}
	else
	{
		shadowFactor = 1.0;
	}

	vec3 lightDir = -lightdir;
	vec3 lightColor = vec3(1.0);

	vec3 normal = normalize(vertex_normal);

	float light = dot(lightDir, normal);	
	vec3 diffuseColor = clamp(light, 0.0f, max_light) * lightColor * shadowFactor;

	//specular light
	vec3 camvec = normalize(camPos - vertex_pos);
	vec3 h = normalize(camvec + lightDir);

	float spec = pow(dot(h, normal), 50);
	vec3 specColor = clamp(spec, 0.0f, max_light) * lightColor * shadowFactor;

	const float ambient = .4 * texture(ssao, fragTex).r;
	//const float ambient = .1;

	no_tonemapping.rgb = (diffuseColor + specColor + ambient) * texturecolor;
	no_tonemapping.a = 1.0f;
}

void getNoLighting()
{
	vec3 texturecolor = texture(colortex, fragTex).rgb;
	no_tonemapping.rgb = texturecolor;
	no_tonemapping.a = 1;
}

vec3 change_luminance(vec3 c_in, float l_out)
{
    float l_in = luminance(c_in);
    return c_in * (l_out / l_in);
}

vec3 reinhard_extended_luminance(vec3 v, float max_white_l)
{
    float l_old = luminance(v);
    float numerator = l_old * (1.0f + (l_old / (max_white_l * max_white_l)));
    float l_new = numerator / (1.0f + l_old);
    return change_luminance(v, l_new);
}

vec3 uncharted2_tonemap_partial(vec3 x)
{
    float A = 0.15f;
    float B = 0.50f;
    float C = 0.10f;
    float D = 0.20f;
    float E = 0.02f;
    float F = 0.30f;
    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

vec3 uncharted2_filmic(vec3 v)
{
    float exposure_bias = 2.0f;
	// function where if the max is low, then the output is high
	float e_fact = 1.3 + (smoothstep(.97, 0.23, maxmin.x) - .2) * .8;
    vec3 curr = uncharted2_tonemap_partial(v * exposure_bias * e_fact);

    vec3 W = vec3(11.2f);
    vec3 white_scale = vec3(1.0f) / uncharted2_tonemap_partial(W);
    return curr * white_scale;
}

void main()
{
	uint lightmaskval = texture(lightmask, fragTex).r;
	if(lightmaskval == 0 || lightmaskval == 2)
	{
		getDefaultLighting();
	}
	else if(lightmaskval == 1)
	{
		getNoLighting();
	}

	// do tonemapping, then bloom
	if(tonemappingEnabled == 1)
		color.rgb = reinhard_extended_luminance(no_tonemapping.rgb, length(maxmin));
		//color.rgb = uncharted2_filmic(no_tonemapping.rgb);
	else
		color = no_tonemapping;
	color.a = 1.0;

	if(lightmaskval != 1)
	{
		float brightness = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
		if(brightness > .37)
            bright = vec4(color.rgb, 1.0);
        else
            bright = vec4(0.0, 0.0, 0.0, 1.0);
	}
	// output only ssao 
	//color.rgb = texture(ssao, fragTex).rgb;
	//bright = vec4(0, 0, 0, 1);
}
