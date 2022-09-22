#pragma once
#include <vector>
#include <glm/ext/vector_float3.hpp>
#include <random>
#include <glad/glad.h>
#include "Program.h"
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/geometric.hpp>
#include <iostream>

class SSAO
{
private:
public:
	std::vector<glm::vec3> ssaoNoise;
	std::vector<glm::vec4> ssaoKernel;
	std::default_random_engine generator;
	std::uniform_real_distribution<GLfloat> randomFloats;
	int width, height;
	float radius, bias;
	std::shared_ptr<Program> ssaoProg, ssaoBlurProg;
public:
	GLuint noiseTexture, ssaoFBO, ssaoColorBuffer, ssaoColorBufferBlur, ssaoBlurFBO,
		sampleUBO, hbaoUBO;
	SSAO(int width, int height);
	~SSAO();
	void render_ssao_to_tex(GLuint ao_tex, GLuint postex, GLuint normtex, GLuint quadVAO,
		const glm::mat4 &P, bool ssao);
};