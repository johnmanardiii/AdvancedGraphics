#version  450 core
out vec4 FragColor;

in vec2 fragTex;

uniform sampler2D ssaoInput;

#define BLUR_RANGE 3

void main() 
{
    vec2 texelSize = 1.0 / vec2(textureSize(ssaoInput, 0));
    float result = 0.0;
    for (int x = -BLUR_RANGE; x < BLUR_RANGE; ++x) 
    {
        for (int y = -BLUR_RANGE; y < BLUR_RANGE; ++y) 
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(ssaoInput, fragTex + offset).r;
        }
    }
    // FragColor = result / (4.0 * 4.0);
    FragColor = vec4(vec3(result / float(BLUR_RANGE * BLUR_RANGE * 4)), 1);
    // FragColor = vec4(1, 1, 0, 1);
    // FragColor = vec4(vec3(texture(ssaoInput, fragTex).r), 1);
}  