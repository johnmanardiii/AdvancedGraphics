#include "SSAO.h"
#include <glm/ext/matrix_transform.hpp>

using namespace glm;
using namespace std;

float lerp(float a, float b, float f)
{
    return a + f * (b - a);
}

SSAO::SSAO(int width, int height) : randomFloats(0.0, 1.0)
{
    // initialize ssaoKernel with random values weighted towards the center
    this->width = width;
    this->height = height;
    //this->radius = .499; // non-snap algo
    this->radius = 1.34; // snap
    this->bias = 0.34;
    int numSamples = 0;
    const int sampleSize = 64;
    while (ssaoKernel.size() < sampleSize)
    {
        glm::vec3 sample(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, randomFloats(generator));
        if (length(sample) > 1.0f)
        {
            continue;
        }
        sample = normalize(sample);
        sample *= randomFloats(generator);
        float scale = float(ssaoKernel.size()) / (float)sampleSize;

        // scale samples s.t. they're more aligned to center of kernel
        scale = lerp(0.1f, 1.0f, scale * scale);
        // sample *= scale;
        ssaoKernel.push_back(vec4(sample, 0));
    }
    // generate random floats for the 4x4 noise texture
    for (unsigned int i = 0; i < 16; i++)
    {
        glm::vec3 noise(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, 0.0f); // rotate around z-axis (in tangent space)
        ssaoNoise.push_back(noise);
    }

    // generate HBAO sample dirs as vec4s (but rlly vec2s ;)
    vector<vec4> HbaoSamples;
    const int num_hbao_samples = 16;  // THIS NEEDS TO MATCH THE SHADER
    vec4 base_vec = vec4(1, 0, 0, 0);
    mat4 rotationMatrix = glm::rotate(mat4(1.0), (2.0f * pi<float>()) / (float)num_hbao_samples, vec3(0, 0, 1));
    for (int i = 0; i < num_hbao_samples; i++)
    {
        HbaoSamples.push_back(base_vec);
        base_vec = rotationMatrix * base_vec;
    }
    // generate FBOs
    glGenFramebuffers(1, &ssaoFBO); 
    glGenFramebuffers(1, &ssaoBlurFBO);

    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
    glGenTextures(1, &ssaoColorBuffer);
    glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGenerateMipmap(GL_TEXTURE_2D);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBuffer, 0);


    // and blur stage
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
    glGenTextures(1, &ssaoColorBufferBlur);
    glBindTexture(GL_TEXTURE_2D, ssaoColorBufferBlur);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGenerateMipmap(GL_TEXTURE_2D);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBufferBlur, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);


    glGenTextures(1, &noiseTexture);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, 4, 4, 0, GL_RGB, GL_FLOAT, &ssaoNoise[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    std::string resourceDirectory = "../resources";
    // initialize shaders and programs:
    ssaoProg = make_shared<Program>();
    ssaoProg->setVerbose(true);
    ssaoProg->setShaderNames(resourceDirectory + "/final_vert.glsl", resourceDirectory + "/ssao_frag.glsl");
    if (!ssaoProg->init())
    {
        std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
        exit(1);
    }
    ssaoProg->init();
    ssaoProg->addAttribute("vertPos");
    ssaoProg->addAttribute("vertTex");
    ssaoProg->addUniform("projection");
    ssaoProg->addUniform("noiseScale");
    ssaoProg->addUniform("radius");
    ssaoProg->addUniform("bias");
    ssaoProg->addUniform("is_ssao");

    // set textures in programs to refer to correct texture unit
    glUseProgram(ssaoProg->pid);
    glUniform1i(glGetUniformLocation(ssaoProg->pid, "gPosition"), 0);
    glUniform1i(glGetUniformLocation(ssaoProg->pid, "gNormal"), 1);
    glUniform1i(glGetUniformLocation(ssaoProg->pid, "texNoise"), 2);
    glUniform1i(glGetUniformLocation(ssaoProg->pid, "ao_radius"), 3);

    // upload kernel sampling data as a uniform buffer:
    GLuint uboIndex = glGetUniformBlockIndex(ssaoProg->pid, "Samples");
    GLint uboSize;
    glGetActiveUniformBlockiv(ssaoProg->pid, uboIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &uboSize);
    glGenBuffers(1, &sampleUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, sampleUBO);
    glBufferData(GL_UNIFORM_BUFFER, uboSize, NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, uboIndex, sampleUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, sampleUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::vec4)* ssaoKernel.size(), &ssaoKernel[0]);
    glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::vec4)* ssaoKernel.size(), sizeof(glm::vec4) * HbaoSamples.size(),
        &HbaoSamples[0]);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    ssaoBlurProg = make_shared<Program>();
    ssaoBlurProg->setVerbose(true);
    ssaoBlurProg->setShaderNames(resourceDirectory + "/final_vert.glsl", resourceDirectory + "/ssao_frag_blur.glsl");
    if (!ssaoBlurProg->init())
    {
        std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
        exit(1);
    }
    ssaoBlurProg->init();
    ssaoBlurProg->addAttribute("vertPos");
    ssaoBlurProg->addAttribute("vertTex");

    glUseProgram(ssaoBlurProg->pid);
    glUniform1i(glGetUniformLocation(ssaoBlurProg->pid, "ssaoInput"), 0);
}

SSAO::~SSAO()
{
    glDeleteTextures(1, &ssaoColorBuffer);
    glDeleteTextures(1, &ssaoColorBufferBlur);
    glDeleteTextures(1, &noiseTexture);
    glDeleteFramebuffers(1, &ssaoBlurFBO);
    glDeleteFramebuffers(1, &ssaoFBO);
}

void SSAO::render_ssao_to_tex(GLuint ao_tex, GLuint postex, GLuint normtex, GLuint quadVAO, const mat4 &P, bool is_ssao)
{
    static int counter = 0;
    counter++;
    if (counter % 15 == 0)
    {
        if (is_ssao)
        {
            //cout << "Screen Space Ambient Occlusion" << endl;
        }
        else
        {
            //cout << "Horizon Based Ambient Occlusion" << endl;
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
    glClear(GL_COLOR_BUFFER_BIT);

    // use the ssao shader to output and sample from each fragment
    ssaoProg->bind();
    // upload uniforms:
    glUniformMatrix4fv(ssaoProg->getUniform("projection"), 1, GL_FALSE, &P[0][0]);
    glUniform2f(ssaoProg->getUniform("noiseScale"), width / 4.0, height / 4.0);
    glUniform1f(ssaoProg->getUniform("radius"), radius);
    glUniform1f(ssaoProg->getUniform("bias"), bias);
    glUniform1i(ssaoProg->getUniform("is_ssao"), is_ssao);

    // bind textures:
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, postex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normtex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, ao_tex);
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    // unbind program
    ssaoProg->unbind();

    glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
    //glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    //glClear(GL_DEPTH_BUFFER_BIT);

    ssaoBlurProg->bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    ssaoBlurProg->unbind();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}