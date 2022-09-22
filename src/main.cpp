/*
CPE/CSC 474 Lab base code Eckhardt/Dahl
based on CPE/CSC 471 Lab base code Wood/Dunn/Eckhardt
*/

#include <iostream>
#include <glad/glad.h>
#include <time.h>
#include "SmartTexture.h"
#include "GLSL.h"
#include "Program.h"
#include "WindowManager.h"
#include "Shape.h"
#include "skmesh.h"

// value_ptr for glm
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

// assimp
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#include "assimp/vector3.h"
#include "assimp/scene.h"
#include <assimp/mesh.h>

#include "stb_image.h"
#include "camera.h"
#include "Character.h"
#include "SSAO.h"


class Character;
class camera;
using namespace std;
using namespace glm;
using namespace Assimp;


class fpscam
{
public:
	float pitch, yaw;
	vec3 direction, pos, cameraUp, cameraFront;
	bool firstMouse;
	int w, a, s, d, space;
	fpscam()
	{
		cameraUp = vec3(0, 1, 0);
		cameraFront = vec3(0, 0, -1);
		firstMouse = true;
		w = a = s = d = 0;
		space = 1;
		//pos = glm::vec3(52.52, -15.884, -4877.996);
		pos = vec3(25, 20.92, 92.17);
		yaw = -142.f;
		pitch = 15.4f;
	}

	glm::mat4 process(double ftime)
	{
		float speed = 4 * ftime;
		if (w == 1)
		{
			pos += speed * cameraFront;
		}
		if (s == 1)
		{
			pos -= speed * cameraFront;
		}
		vec3 side = normalize(cross(cameraFront, cameraUp));
		if (a == 1)
		{
			pos -= side * speed;
		}
		if (d == 1)
		{
			pos += side * speed;
		}


		direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		direction.y = sin(glm::radians(pitch));
		direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

		cameraFront = normalize(direction);

		return lookAt(pos, pos + cameraFront, cameraUp);
	}
};

fpscam fcam;
float lastX, lastY;

void mouse_curs_callback(GLFWwindow* window, double xpos, double ypos)
{
	if (fcam.firstMouse) // initially set to true
	{
		lastX = xpos;
		lastY = ypos;
		fcam.firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = ypos - lastY;
	lastX = xpos;
	lastY = ypos;
	const float sensitivity = .1f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	fcam.yaw += xoffset;
	fcam.pitch -= yoffset;

	if (fcam.pitch > 89.0f)
		fcam.pitch = 89.0f;
	if (fcam.pitch < -89.0f)
		fcam.pitch = -89.0f;
}

#define SHADOW_DIM 8192
#define FFACT 100000

double get_last_elapsed_time()
{
	static double lasttime = glfwGetTime();
	double actualtime = glfwGetTime();
	double difference = actualtime - lasttime;
	static double last_diff = difference;
	lasttime = actualtime;
	if (difference == 0)
	{
		difference = last_diff;
	}
	last_diff = difference;

	if (difference == 0.0)
	{
		cout << "nahh" << endl;
	}
	return difference;
}

struct Light
{
	vec3 position;
	vec3 direction;
	vec3 color;
};

class ssbo_data
{
	/*
	 * ints because atomic operations really only work on ints
	 * 
	 */
public:
	int maxLum = INT_MIN;
	int minLum = INT_MAX;
	int avgLum = 0;
};

class Application : public EventCallbacks
{
public:
	WindowManager * windowManager = nullptr;

	// Our shader program
	std::shared_ptr<Program> psky, skinProg, pplane, katanaprog, treeprog, post_processing, shadowProg, treeShadowProg,
							ninjaShadowProg, final_prog, bloom_blur_prog;

	// compute shader program
	GLuint maxMinProgram, ssbo_GPU_id;
	ssbo_data ssbo_CPUMEM;

	// skinnedMesh
	SkinnedMesh skmesh;
	// textures
	shared_ptr<SmartTexture> skyTex;

	GLuint VertexArrayIDBox, VertexBufferIDBox, VertexBufferTex;
	GLuint base_fbo, depth_rb;
	GLuint color_tex, pos_tex, norm_tex, mask_tex, view_pos_tex, view_norm_tex, ao_radius_tex;

	// Bloom ids:
	GLuint first_bloom_pass_fbo;
	GLuint colorBuffers[2], non_tm;
	GLuint pingpongFBO[2];
	GLuint pingpongBuffer[2];

	// shadow map framebuffer data:
	GLuint FBOtex_shadowMapDepth, fb_shadowMap;
	
	// shapes
	shared_ptr<Shape> skyShape, desert, katana, tree;
	Character ninjaCharacter;
	shared_ptr<camera> mycam;
	SSAO* ssao;

	double totaltime = 0;
	float armRot = 0.0f;
	vec2 exposure = vec2(1);
	int shadowsEnabled = 1, bloomEnabled = 1, ninjaCam = 1, tonemappingEnabled = 1;
	float last_frametime = 0.0;
	mat4 lastUsedProj, lastUsedVCam;

	Light primaryLight;

	Application::Application()
	{
		ninjaCharacter = Character{};
		mycam = make_shared<camera>(&ninjaCharacter);
	};

	~Application()
	{
		delete ssao;
	}
	
	void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
	{
		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		{
			glfwSetWindowShouldClose(window, GL_TRUE);
		}
		
		if (key == GLFW_KEY_W && action == GLFW_PRESS)
		{
			mycam->w = 1;
		}
		else if (key == GLFW_KEY_W && action == GLFW_RELEASE)
		{
			mycam->w = 0;
		}
		if (key == GLFW_KEY_S && action == GLFW_PRESS)
		{
			mycam->s = 1;
		}
		else if (key == GLFW_KEY_S && action == GLFW_RELEASE)
		{
			mycam->s = 0;
		}
		if (key == GLFW_KEY_A && action == GLFW_PRESS)
		{
			mycam->a = 1;
		}
		else if (key == GLFW_KEY_A && action == GLFW_RELEASE)
		{
			mycam->a = 0;
		}
		if (key == GLFW_KEY_D && action == GLFW_PRESS)
		{
			mycam->d = 1;
		}
		else if (key == GLFW_KEY_D && action == GLFW_RELEASE)
		{
			mycam->d = 0;
		}

		if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
		{
			mycam->arm_raised = !mycam->arm_raised;
		}
		
		if ((key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT) && action == GLFW_PRESS)
		{
			mycam->running = !mycam->running;
		}

		if (key == GLFW_KEY_I && action == GLFW_PRESS)
		{
			fcam.w = 1;
		}
		if (key == GLFW_KEY_I && action == GLFW_RELEASE)
		{
			fcam.w = 0;
		}
		if (key == GLFW_KEY_K && action == GLFW_PRESS)
		{
			fcam.s = 1;
		}
		if (key == GLFW_KEY_K && action == GLFW_RELEASE)
		{
			fcam.s = 0;
		}
		if (key == GLFW_KEY_J && action == GLFW_PRESS)
		{
			fcam.a = 1;
		}
		if (key == GLFW_KEY_J && action == GLFW_RELEASE)
		{
			fcam.a = 0;
		}
		if (key == GLFW_KEY_L && action == GLFW_PRESS)
		{
			fcam.d = 1;
		}
		if (key == GLFW_KEY_L && action == GLFW_RELEASE)
		{
			fcam.d = 0;
		}
		if (key == GLFW_KEY_1 && action == GLFW_PRESS)
		{
			ninjaCam = true;
		}
		if (key == GLFW_KEY_2 && action == GLFW_PRESS)
		{
			ninjaCam = false;
		}
		if (key == GLFW_KEY_3 && action == GLFW_PRESS)
		{
			ssao->radius += .05;
		}
		if (key == GLFW_KEY_4 && action == GLFW_PRESS)
		{
			ssao->radius -= .05;
		}
		if (key == GLFW_KEY_5 && action == GLFW_PRESS)
		{
			ssao->bias += .01;
		}
		if (key == GLFW_KEY_6 && action == GLFW_PRESS)
		{
			ssao->bias -= .01;
		}


		// lighting settings
		if (key == GLFW_KEY_Z && action == GLFW_PRESS)
		{
			shadowsEnabled = !shadowsEnabled;
		}
		if (key == GLFW_KEY_X && action == GLFW_PRESS)
		{
			bloomEnabled = !bloomEnabled;
		}
		if (key == GLFW_KEY_C && action == GLFW_PRESS)
		{
			tonemappingEnabled = !tonemappingEnabled;
		}
	}

	// callback for the mouse when clicked move the triangle when helper functions
	// written
	void mouseCallback(GLFWwindow *window, int button, int action, int mods)
	{

	}

	//if the window is resized, capture the new size and reset the viewport
	void resizeCallback(GLFWwindow *window, int in_width, int in_height)
	{
		//get the window size - may be different then pixels for retina
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		glViewport(0, 0, width, height);
	}
	/*Note that any gl calls must always happen after a GL state is initialized */
	
	
	/*Note that any gl calls must always happen after a GL state is initialized */
	void initGeom(const std::string& resourceDirectory)
	{
		primaryLight.position = vec3(-72, 33, -65);
		primaryLight.direction = normalize(vec3(.716, -.338, .61));
		primaryLight.color = vec3(1.0f, 1.0f, 1.0f);

		
		//init rectangle mesh (2 triangles) for the post processing
		glGenVertexArrays(1, &VertexArrayIDBox);
		glBindVertexArray(VertexArrayIDBox);

		//generate vertex buffer to hand off to OGL
		glGenBuffers(1, &VertexBufferIDBox);
		//set the current state to focus on our vertex buffer
		glBindBuffer(GL_ARRAY_BUFFER, VertexBufferIDBox);

		GLfloat* rectangle_vertices = new GLfloat[18];
		// front
		int verccount = 0;

		rectangle_vertices[verccount++] = -1.0, rectangle_vertices[verccount++] = -1.0, rectangle_vertices[verccount++] = 0.0;
		rectangle_vertices[verccount++] = 1.0, rectangle_vertices[verccount++] = -1.0, rectangle_vertices[verccount++] = 0.0;
		rectangle_vertices[verccount++] = -1.0, rectangle_vertices[verccount++] = 1.0, rectangle_vertices[verccount++] = 0.0;
		rectangle_vertices[verccount++] = 1.0, rectangle_vertices[verccount++] = -1.0, rectangle_vertices[verccount++] = 0.0;
		rectangle_vertices[verccount++] = 1.0, rectangle_vertices[verccount++] = 1.0, rectangle_vertices[verccount++] = 0.0;
		rectangle_vertices[verccount++] = -1.0, rectangle_vertices[verccount++] = 1.0, rectangle_vertices[verccount++] = 0.0;


		//actually memcopy the data - only do this once
		glBufferData(GL_ARRAY_BUFFER, 18 * sizeof(float), rectangle_vertices, GL_STATIC_DRAW);
		//we need to set up the vertex array
		glEnableVertexAttribArray(0);
		//key function to get up how many elements to pull out at a time (3)
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);


		//generate vertex buffer to hand off to OGL
		glGenBuffers(1, &VertexBufferTex);
		//set the current state to focus on our vertex buffer
		glBindBuffer(GL_ARRAY_BUFFER, VertexBufferTex);

		float t = 1. / 100.;
		GLfloat* rectangle_texture_coords = new GLfloat[12];
		int texccount = 0;
		rectangle_texture_coords[texccount++] = 0, rectangle_texture_coords[texccount++] = 0;
		rectangle_texture_coords[texccount++] = 1, rectangle_texture_coords[texccount++] = 0;
		rectangle_texture_coords[texccount++] = 0, rectangle_texture_coords[texccount++] = 1;
		rectangle_texture_coords[texccount++] = 1, rectangle_texture_coords[texccount++] = 0;
		rectangle_texture_coords[texccount++] = 1, rectangle_texture_coords[texccount++] = 1;
		rectangle_texture_coords[texccount++] = 0, rectangle_texture_coords[texccount++] = 1;

		//actually memcopy the data - only do this once
		glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(float), rectangle_texture_coords, GL_STATIC_DRAW);
		//we need to set up the vertex array
		glEnableVertexAttribArray(2);
		//key function to get up how many elements to pull out at a time (2)
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

		if (!skmesh.LoadMesh(resourceDirectory + "/Ninja.fbx")) {
			printf("Mesh load failed\n");
			return;
			}

		// Initialize mesh.
		skyShape = make_shared<Shape>();
		skyShape->loadMesh(resourceDirectory + "/sphere.obj");
		skyShape->resize();
		skyShape->init();

		string mtldir = resourceDirectory + "/katana/";
		katana = make_shared<Shape>();
		katana->loadMesh(resourceDirectory + "/katana/katana.obj", &mtldir, stbi_load);
		// katana->resize();	// probably don't want to resize because i placed origin of model at hand point
		katana->init();

		desert = make_shared<Shape>();
		mtldir = resourceDirectory + "/city/";
		desert->loadMesh(resourceDirectory + "/city/city.obj", &mtldir, stbi_load);
		desert->resize();
		desert->init();

		tree = make_shared<Shape>();
		mtldir = resourceDirectory + "/tree/";
		tree->loadMesh(resourceDirectory + "/tree/tree.obj", &mtldir, stbi_load);
		tree->resize();
		tree->init();

		// sky texture
		auto strSky = resourceDirectory + "/darksky.jpg";
		skyTex = SmartTexture::loadTexture(strSky, true);
		if (!skyTex)
			cerr << "error: texture " << strSky << " not found" << endl;

		GLuint Tex1Location = glGetUniformLocation(katanaprog->pid, "tex");//tex, tex2... sampler in the fragment shader
		// Then bind the uniform samplers to texture units:
		glUseProgram(katanaprog->pid);
		glUniform1i(Tex1Location, 0);

		glUseProgram(pplane->pid);
		glUniform1i(Tex1Location, 0);

		Tex1Location = glGetUniformLocation(treeprog->pid, "tex");//tex, tex2... sampler in the fragment shader
		glUseProgram(treeprog->pid);
		glUniform1i(Tex1Location, 0);

		// associate textures in shaders with texture numbers for use later
		glUseProgram(post_processing->pid);
		glUniform1i(glGetUniformLocation(post_processing->pid, "colortex"), 0);
		glUniform1i(glGetUniformLocation(post_processing->pid, "postex"), 1);
		glUniform1i(glGetUniformLocation(post_processing->pid, "normtex"), 2);
		glUniform1i(glGetUniformLocation(post_processing->pid, "lightmask"), 3);
		glUniform1i(glGetUniformLocation(post_processing->pid, "shadowMapTex"), 4);
		glUniform1i(glGetUniformLocation(post_processing->pid, "ssao"), 5);
		glUniform1i(glGetUniformLocation(post_processing->pid, "viewPos"), 6);
		glUniform1i(glGetUniformLocation(post_processing->pid, "viewNorm"), 7);

		glUseProgram(final_prog->pid);
		glUniform1i(glGetUniformLocation(final_prog->pid, "colortex"), 0);
		glUniform1i(glGetUniformLocation(final_prog->pid, "brighttex"), 1);
		glUniform1i(glGetUniformLocation(final_prog->pid, "lightmaskval"), 2);
		glUniform1i(glGetUniformLocation(final_prog->pid, "viewPosTex"), 3);
		glUniform1i(glGetUniformLocation(final_prog->pid, "viewNormTex"), 4);

		glUseProgram(bloom_blur_prog->pid);
		glUniform1i(glGetUniformLocation(bloom_blur_prog->pid, "brighttex"), 0);

		glUseProgram(psky->pid);
		glUniform1i(glGetUniformLocation(psky->pid, "tex"), 0);

		// set up framebuffer objects and textures for deferred shading:
		int width, height, channels;
		glfwGetFramebufferSize(windowManager->getHandle(), &width, &height);
		//RGBA8 2D texture, 24 bit depth texture, 256x256
		glGenTextures(1, &color_tex);
		glBindTexture(GL_TEXTURE_2D, color_tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glGenerateMipmap(GL_TEXTURE_2D);

		glGenTextures(1, &pos_tex);
		glBindTexture(GL_TEXTURE_2D, pos_tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
		glGenerateMipmap(GL_TEXTURE_2D);

		glGenTextures(1, &norm_tex);
		glBindTexture(GL_TEXTURE_2D, norm_tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
		glGenerateMipmap(GL_TEXTURE_2D);

		// TODO: update these with new values
		glGenTextures(1, &view_pos_tex);
		glBindTexture(GL_TEXTURE_2D, view_pos_tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
		glGenerateMipmap(GL_TEXTURE_2D);

		glGenTextures(1, &view_norm_tex);
		glBindTexture(GL_TEXTURE_2D, view_norm_tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
		glGenerateMipmap(GL_TEXTURE_2D);

		glGenTextures(1, &mask_tex);
		glBindTexture(GL_TEXTURE_2D, mask_tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, width, height, 0, GL_RED_INTEGER, 
			GL_UNSIGNED_INT, NULL);
		glGenerateMipmap(GL_TEXTURE_2D);

		glGenTextures(1, &ao_radius_tex);
		glBindTexture(GL_TEXTURE_2D, ao_radius_tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
		glGenerateMipmap(GL_TEXTURE_2D);

		// check if I generated these textures right, then move on to working on the FBOs
		glGenFramebuffers(1, &base_fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, base_fbo);
		//Attach 2D textures to this FBO
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, pos_tex, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, norm_tex, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, mask_tex, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, view_pos_tex, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, GL_TEXTURE_2D, view_norm_tex, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT6, GL_TEXTURE_2D, ao_radius_tex, 0);


		GLuint out_positions[7] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5, GL_COLOR_ATTACHMENT6};
		// set openGL to draw to 4 texture objects on this framebuffer
		glDrawBuffers(7, out_positions);
		
		//-------------------------
		glGenRenderbuffers(1, &depth_rb);
		glBindRenderbuffer(GL_RENDERBUFFER, depth_rb);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, width, height);
		//-------------------------
		//Attach depth buffer to FBO
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rb);
		//-------------------------
		//Does the GPU support current FBO configuration?
		GLenum status;
		status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		switch (status)
		{
		case GL_FRAMEBUFFER_COMPLETE:
			cout << "status framebuffer: good";
			break;
		default:
			cout << "status framebuffer: bad!!!!!!!!!!!!!!!!!!!!!!!!!";
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);


		// Attach Shadow Map depth texture to Shadow Map FBO
		{
			glGenFramebuffers(1, &fb_shadowMap);
			glBindFramebuffer(GL_FRAMEBUFFER, fb_shadowMap);

			glGenTextures(1, &FBOtex_shadowMapDepth);
			//glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, FBOtex_shadowMapDepth);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, SHADOW_DIM, SHADOW_DIM, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
			glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, glm::value_ptr(vec3(1.0)));

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, FBOtex_shadowMapDepth, 0);

			// We don't want the draw result for a shadow map!
			glDrawBuffer(GL_NONE);
			glReadBuffer(GL_NONE);

			GLenum status;
			status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			switch (status)
			{
			case GL_FRAMEBUFFER_COMPLETE:
				cout << "status framebuffer: good" << std::endl;
				break;
			default:
				cout << "status framebuffer: bad!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
			}
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}


		// generate framebuffers for bloom:
		glGenFramebuffers(1, &first_bloom_pass_fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, first_bloom_pass_fbo);
		// create 2 floating point color buffers (1 for normal rendering, other for brightness threshold values)
		glGenTextures(2, colorBuffers);
		for (unsigned int i = 0; i < 2; i++)
		{
			glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);  // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			// attach texture to framebuffer
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
		}

		// generate additional texture for non-tonemapped image
		glGenTextures(1, &non_tm);
		glBindTexture(GL_TEXTURE_2D, non_tm);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, non_tm, 0);
		
		// dont need to create and attach depth buffer (renderbuffer) because only one post-processing render quad
		
		// tell OpenGL which color attachments we'll use (of this framebuffer) for rendering 
		unsigned int attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
		glDrawBuffers(3, attachments);
		// finally check if framebuffer is complete
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			std::cout << "Framebuffer not complete!" << std::endl;
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glGenFramebuffers(2, pingpongFBO);
		glGenTextures(2, pingpongBuffer);
		for (unsigned int i = 0; i < 2; i++)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
			glBindTexture(GL_TEXTURE_2D, pingpongBuffer[i]);
			glTexImage2D(
				GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL
			);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
			glFramebufferTexture2D(
				GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongBuffer[i], 0
			);
		}
	}

	void initComputeShaders()
	{
		string shaderString = readFileAsString("../resources/compute_max_min.glsl");
		const char* shader = shaderString.c_str();
		GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);
		glShaderSource(computeShader, 1, &shader, nullptr);

		GLint rc;
		CHECKED_GL_CALL(glCompileShader(computeShader));
		CHECKED_GL_CALL(glGetShaderiv(computeShader, GL_COMPILE_STATUS, &rc));
		if (!rc)	//error compiling the shader file
		{
			GLSL::printShaderInfoLog(computeShader);
			std::cout << "Error compiling compute shader " << std::endl;
			exit(1);
		}

		maxMinProgram = glCreateProgram();
		glAttachShader(maxMinProgram, computeShader);
		glLinkProgram(maxMinProgram);

		// generate buffer to use for ssbo
		glGenBuffers(1, &ssbo_GPU_id);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_GPU_id);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(ssbo_data), &ssbo_CPUMEM, GL_DYNAMIC_COPY);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_GPU_id);
		
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}

	//General OGL initialization - set OGL state here
	void init(const std::string& resourceDirectory)
	{
		int width, height;
		glfwSetCursorPosCallback(windowManager->getHandle(), mouse_curs_callback);
		glfwSetInputMode(windowManager->getHandle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		glfwGetFramebufferSize(windowManager->getHandle(), &width, &height);
		lastX = width / 2.0f;
		lastY = height / 2.0f;
		GLSL::checkVersion();
		// initialize character
		ninjaCharacter.cam = mycam;
		// initialize SSAO
		ssao = new SSAO(width, height);
		
		// Set background color.
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		// Enable z-buffer test.
		glEnable(GL_DEPTH_TEST);
		//glDisable(GL_DEPTH_TEST);
		// Initialize the GLSL program.

		post_processing = make_shared<Program>();
		post_processing->setVerbose(true);
		post_processing->setShaderNames(resourceDirectory + "/pp_vert.glsl", resourceDirectory + "/pp_frag.glsl");
		if (!post_processing->init())
		{
			std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
			exit(1);
		}
		post_processing->init();
		post_processing->addUniform("lightSpace");
		post_processing->addAttribute("vertPos");
		post_processing->addAttribute("vertTex");
		post_processing->addUniform("camPos");
		post_processing->addUniform("lightpos");
		post_processing->addUniform("lightdir");
		post_processing->addUniform("shadowsEnabled");
		post_processing->addUniform("tonemappingEnabled");
		post_processing->addUniform("maxmin");

		final_prog = make_shared<Program>();
		final_prog->setVerbose(true);
		final_prog->setShaderNames(resourceDirectory + "/final_vert.glsl", resourceDirectory + "/final_frag.glsl");
		if (!final_prog->init())
		{
			std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
			exit(1);
		}
		final_prog->init();
		final_prog->addAttribute("vertPos");
		final_prog->addAttribute("vertTex");
		final_prog->addUniform("bloomEnabled");
		final_prog->addUniform("P");
		final_prog->addUniform("Vcam");

		bloom_blur_prog = make_shared<Program>();
		bloom_blur_prog->setVerbose(true);
		bloom_blur_prog->setShaderNames(resourceDirectory + "/final_vert.glsl", resourceDirectory + "/frag_bloom_pass.glsl");
		if (!bloom_blur_prog->init())
		{
			std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
			exit(1);
		}
		bloom_blur_prog->init();
		bloom_blur_prog->addAttribute("vertPos");
		bloom_blur_prog->addAttribute("vertTex");
		bloom_blur_prog->addUniform("horizontal");

		// alternate shaders that output to multiple textures
		skinProg = std::make_shared<Program>();
		skinProg->setVerbose(true);
		skinProg->setShaderNames(resourceDirectory + "/skinning_vert.glsl", resourceDirectory + "/preprocess.glsl");
		if (!skinProg->init())
		{
			std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
			exit(1);
		}

		skinProg->addUniform("P");
		skinProg->addUniform("V");
		skinProg->addUniform("M");
		skinProg->addUniform("tex");
		skinProg->addUniform("camPos");
		skinProg->addAttribute("vertPos");
		skinProg->addAttribute("vertNor");
		skinProg->addAttribute("vertTex");
		skinProg->addAttribute("BoneIDs");
		skinProg->addAttribute("Weights");

		pplane = std::make_shared<Program>();
		pplane->setVerbose(true);
		pplane->setShaderNames(resourceDirectory + "/plane_vertex.glsl", resourceDirectory + "/preprocess.glsl");
		if (!pplane->init())
		{
			std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
			exit(1);
		}
		pplane->addUniform("P");
		pplane->addUniform("V");
		pplane->addUniform("M");
		pplane->addUniform("camPos");
		pplane->addAttribute("vertPos");
		pplane->addAttribute("vertNor");
		pplane->addAttribute("vertTex");

		katanaprog = std::make_shared<Program>();
		katanaprog->setVerbose(true);
		katanaprog->setShaderNames(resourceDirectory + "/katana_vert.glsl", resourceDirectory + "/preprocess.glsl");
		if (!katanaprog->init())
		{
			std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
			exit(1);
		}
		katanaprog->addAttribute("vertPos");
		katanaprog->addAttribute("vertNor");
		katanaprog->addAttribute("vertTex");
		katanaprog->addUniform("M");
		katanaprog->addUniform("V");
		katanaprog->addUniform("P");
		katanaprog->addUniform("camPos");		// for specular lighting

		treeprog = std::make_shared<Program>();
		treeprog->setVerbose(true);
		treeprog->setShaderNames(resourceDirectory + "/tree_vert.glsl", resourceDirectory + "/preprocess.glsl");
		if (!treeprog->init())
		{
			std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
			exit(1);
		}
		treeprog->addAttribute("vertPos");
		treeprog->addAttribute("vertNor");
		treeprog->addAttribute("vertTex");
		treeprog->addUniform("M");
		treeprog->addUniform("V");
		treeprog->addUniform("P");
		treeprog->addUniform("camPos");		// for specular lighting
		treeprog->addUniform("glTime");

		// second sky program that specifies that it doesn't want lighting.
		psky = std::make_shared<Program>();
		psky->setVerbose(true);
		psky->setShaderNames(resourceDirectory + "/skyvertex.glsl", resourceDirectory + "/skyfrag_deferred.glsl");
		if (!psky->init())
		{
			std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
			exit(1);
		}

		psky->addUniform("P");
		psky->addUniform("V");
		psky->addUniform("M");
		psky->addUniform("tex");
		psky->addUniform("camPos");
		psky->addAttribute("vertPos");
		psky->addAttribute("vertNor");
		psky->addAttribute("vertTex");

		// render to shadow
		// Initialize the Shadow Map shader program.
		shadowProg = make_shared<Program>();
		shadowProg->setVerbose(true);
		shadowProg->setShaderNames(resourceDirectory + "/shadow_vert.glsl", resourceDirectory + "/shadow_frag.glsl");
		if (!shadowProg->init())
		{
			std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
			exit(1);
		}
		shadowProg->init();
		shadowProg->addUniform("P");
		shadowProg->addUniform("V");
		shadowProg->addUniform("M");
		shadowProg->addAttribute("vertPos");
		shadowProg->addAttribute("vertTex");
		shadowProg->addAttribute("vertNor");
		shadowProg->addAttribute("BoneIDs");
		shadowProg->addAttribute("Weights");


		// need separate programs for the tree and the ninja since their vertices are animated in the vertex shader
		treeShadowProg = make_shared<Program>();
		treeShadowProg->setVerbose(true);
		treeShadowProg->setShaderNames(resourceDirectory + "/shadow_tree_vert.glsl", resourceDirectory + "/shadow_frag.glsl");
		if (!treeShadowProg->init())
		{
			std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
			exit(1);
		}
		treeShadowProg->addAttribute("vertPos");
		treeShadowProg->addAttribute("vertNor");
		treeShadowProg->addAttribute("vertTex");
		treeShadowProg->addUniform("M");
		treeShadowProg->addUniform("V");
		treeShadowProg->addUniform("P");
		treeShadowProg->addUniform("glTime");

		ninjaShadowProg = std::make_shared<Program>();
		ninjaShadowProg->setVerbose(true);
		ninjaShadowProg->setShaderNames(resourceDirectory + "/shadow_skinning_vert.glsl",
			resourceDirectory + "/shadow_frag.glsl");
		if (!ninjaShadowProg->init())
		{
			std::cerr << "One or more shaders failed to compile... exiting!" << std::endl;
			exit(1);
		}

		ninjaShadowProg->addUniform("P");
		ninjaShadowProg->addUniform("V");
		ninjaShadowProg->addUniform("M");
		ninjaShadowProg->addAttribute("vertPos");
		ninjaShadowProg->addAttribute("vertNor");
		ninjaShadowProg->addAttribute("vertTex");
		ninjaShadowProg->addAttribute("BoneIDs");
		ninjaShadowProg->addAttribute("Weights");

		initComputeShaders();
	}

	void render_to_texture()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, base_fbo);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		double frametime = get_last_elapsed_time();
		last_frametime = frametime;
		totaltime += frametime;

		// update actors in scene
		int animation_state = ninjaCharacter.update(frametime, skmesh);
		skmesh.SetNextAnimation(animation_state);

		// Get current frame buffer size.
		int width, height;
		glfwGetFramebufferSize(windowManager->getHandle(), &width, &height);
		float aspect = width / (float)height;
		glViewport(0, 0, width, height);

		// Clear framebuffer.
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Create the matrix stacks - please leave these alone for now

		
		glm::mat4 V, M, P; //View, Model and Perspective matrix
		vec3 campos;
		if(ninjaCam)
		{
			V = mycam->process(frametime);
			campos = mycam->pos;
		}
		else
		{
			V = fcam.process(frametime);
			campos = fcam.pos;
		}
		
		M = glm::mat4(1);
		// ...but we overwrite it (optional) with a perspective projection.
		P = glm::perspective((float)(3.14159 / 4.) + radians((abs(ninjaCharacter.current_speed) - 4) * 1 + 2), (float)((float)width / (float)height), .2f, 140.0f); //so much type casting... GLM metods are quite funny ones
		lastUsedProj = P;
		lastUsedVCam = V;
		auto sangle = -3.1415926f / 2.0f;
		glm::mat4 RotateXSky = glm::rotate(glm::mat4(1.0f), sangle, glm::vec3(1.0f, 0.0f, 0.0f));
		glm::vec3 camp = campos;
		glm::mat4 TransSky = glm::translate(glm::mat4(1.0f), camp);
		glm::mat4 SSky = glm::scale(glm::mat4(1.0f), glm::vec3(4));

		M = TransSky * RotateXSky * SSky;

		// Draw the sky using GLSL.
		psky->bind();
		GLuint texLoc = glGetUniformLocation(psky->pid, "tex");
		skyTex->bind(texLoc);
		glUniformMatrix4fv(psky->getUniform("P"), 1, GL_FALSE, &P[0][0]);
		glUniformMatrix4fv(psky->getUniform("V"), 1, GL_FALSE, &V[0][0]);
		glUniformMatrix4fv(psky->getUniform("M"), 1, GL_FALSE, &M[0][0]);
		glUniform3fv(psky->getUniform("camPos"), 1, &campos[0]);

		glDisable(GL_DEPTH_TEST);
		skyShape->draw(psky, false);
		glEnable(GL_DEPTH_TEST);
		skyTex->unbind();
		psky->unbind();

		

		// draw the skinned mesh		
		skinProg->bind();
		texLoc = glGetUniformLocation(skinProg->pid, "tex");
		glm::mat4 Trans = glm::translate(glm::mat4(1.0f), ninjaCharacter.charPos);
		glm::mat4 Scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.03f, 0.03f, 0.03f));
		mat4 ninjaRot = glm::rotate(mat4(1), glm::radians(ninjaCharacter.yrot), glm::vec3(0, 1, 0));
		mat4 ninja_transform = Trans * ninjaRot * Scale;

		glUniform3fv(skinProg->getUniform("camPos"), 1, &campos[0]);
		glUniformMatrix4fv(skinProg->getUniform("P"), 1, GL_FALSE, &P[0][0]);
		glUniformMatrix4fv(skinProg->getUniform("V"), 1, GL_FALSE, &V[0][0]);
		glUniformMatrix4fv(skinProg->getUniform("M"), 1, GL_FALSE, &ninja_transform[0][0]);
		skmesh.setBoneTransformations(skinProg->pid, frametime * 19., ninjaCharacter.current_speed);
		skmesh.Render(texLoc);
		skinProg->unbind();

		// draw desert environment
		pplane->bind();
		glUniform3fv(pplane->getUniform("camPos"), 1, &campos[0]);
		M = translate(mat4(1), vec3(0, 27, 0)) * rotate(mat4(1), pi<float>(), vec3(0, 1, 0)) *
			scale(mat4(1), vec3(100.0f));
		glUniformMatrix4fv(pplane->getUniform("P"), 1, GL_FALSE, &P[0][0]);
		glUniformMatrix4fv(pplane->getUniform("V"), 1, GL_FALSE, &V[0][0]);
		glUniformMatrix4fv(pplane->getUniform("M"), 1, GL_FALSE, &M[0][0]);
		desert->draw(pplane, false);
		pplane->unbind();

		treeprog->bind();

		glUniform3fv(treeprog->getUniform("camPos"), 1, &campos[0]);
		glUniformMatrix4fv(treeprog->getUniform("P"), 1, GL_FALSE, &P[0][0]);
		glUniformMatrix4fv(treeprog->getUniform("V"), 1, GL_FALSE, &V[0][0]);
		glUniform1f(treeprog->getUniform("glTime"), totaltime);
		M = translate(mat4(1), vec3(13, 1, -3)) * scale(mat4(1), vec3(10));
		glUniformMatrix4fv(treeprog->getUniform("M"), 1, GL_FALSE, &M[0][0]);
		tree->draw(treeprog, false);

		M = translate(mat4(1), vec3(-20, 1, -3)) * scale(mat4(1), vec3(10));
		glUniformMatrix4fv(treeprog->getUniform("M"), 1, GL_FALSE, &M[0][0]);
		tree->draw(treeprog, false);

		treeprog->unbind();

		katanaprog->bind();
		glUniform3fv(katanaprog->getUniform("camPos"), 1, &campos[0]);

		mat4 right_hand = skmesh.GetMountMatrix("hand.R");

		// raise or lower arm based on whether or not space bar is toggled:
		float goal = 0.0;
		if (mycam->arm_raised)
		{
			goal = pi<float>() - .4f;
		}
		armRot = exponential_growth(armRot, goal, .03f * 60.0f, frametime);	// even using exponential function here ;)
		mat4 rotate_arm = rotate(mat4(1), armRot, vec3(1, .3, 0));
		skmesh.armRaised = mycam->arm_raised;
		skmesh.AddBoneTransformation("upper_arm.R", rotate_arm);


		M = ninja_transform * right_hand * rotate(mat4(1), pi<float>() / 3.0f, vec3(1, 0, 0)) * translate(mat4(1), vec3(-.012, 0, -.011))
			* rotate(mat4(1), -pi<float>() / 4.0f, vec3(0, 1, 0)) * scale(mat4(1), vec3(.02f));
		glUniformMatrix4fv(katanaprog->getUniform("P"), 1, GL_FALSE, &P[0][0]);
		glUniformMatrix4fv(katanaprog->getUniform("V"), 1, GL_FALSE, &V[0][0]);
		glUniformMatrix4fv(katanaprog->getUniform("M"), 1, GL_FALSE, &M[0][0]);
		katana->draw(katanaprog, false);
		katanaprog->unbind();
	}

	void render_ssao()
	{
		ssao->render_ssao_to_tex(ao_radius_tex, view_pos_tex, view_norm_tex, VertexArrayIDBox, lastUsedProj,
			true);
	}

	void get_light_proj_matrix(glm::mat4& lightP)
	{
		// If your scene goes outside these "bounds" (e.g. shadows stop working near boundary),
		// feel free to increase these numbers (or decrease if scene shrinks/light gets closer to
		// scene objects).
		const float left = -150.0f;
		const float right = 150.0f;
		const float bottom = -100.0f;
		const float top = 100.0f;
		const float zNear = -60.f;
		const float zFar = 220.0f;

		lightP = glm::ortho(left, right, bottom, top, zNear, zFar);
	}

	void get_light_view_matrix(glm::mat4& lightV)
	{
		// Change earth_pos (or primaryLight.direction) to change where the light is pointing at.
		lightV = glm::lookAt(primaryLight.position, primaryLight.position + primaryLight.direction, glm::vec3(0.0f, 1.0f, 0.0f));
	}

	void render_to_shadowmap()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, fb_shadowMap);
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_DEPTH_BUFFER_BIT);

		glViewport(0, 0, SHADOW_DIM, SHADOW_DIM);

		glDisable(GL_BLEND);

		glm::mat4 M, V, S, T, P;

		// Orthographic frustum in light space; encloses the scene, adjust if larger or smaller scene used.
		get_light_proj_matrix(P);

		// "Camera" for rendering shadow map is at light source, looking at the scene.
		get_light_view_matrix(V);

		// draw the ninja with its own shaders since its vertices move
		// draw the skinned mesh

		
		// draw the skinned mesh		
		ninjaShadowProg->bind();
		glm::mat4 Trans = glm::translate(glm::mat4(1.0f), ninjaCharacter.charPos);
		glm::mat4 Scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.03f, 0.03f, 0.03f));
		mat4 ninjaRot = glm::rotate(mat4(1), glm::radians(ninjaCharacter.yrot), glm::vec3(0, 1, 0));
		mat4 ninja_transform = Trans * ninjaRot * Scale;

		glUniformMatrix4fv(ninjaShadowProg->getUniform("P"), 1, GL_FALSE, &P[0][0]);
		glUniformMatrix4fv(ninjaShadowProg->getUniform("V"), 1, GL_FALSE, &V[0][0]);
		glUniformMatrix4fv(ninjaShadowProg->getUniform("M"), 1, GL_FALSE, &ninja_transform[0][0]);
		skmesh.setBoneTransformations(ninjaShadowProg->pid, 0, ninjaCharacter.current_speed);
		skmesh.Render();
		ninjaShadowProg->unbind();
		

		// Bind shadow map shader program and matrix uniforms.
		shadowProg->bind();
		glUniformMatrix4fv(shadowProg->getUniform("P"), 1, GL_FALSE, &P[0][0]);
		glUniformMatrix4fv(shadowProg->getUniform("V"), 1, GL_FALSE, &V[0][0]);

		// start of drawing objects

		// draw desert environment
		M = translate(mat4(1), vec3(0, 27, 0)) * rotate(mat4(1), pi<float>(), vec3(0, 1, 0)) *
			scale(mat4(1), vec3(100.0f));
		glUniformMatrix4fv(shadowProg->getUniform("M"), 1, GL_FALSE, &M[0][0]);
		desert->draw(shadowProg, false);

		mat4 right_hand = skmesh.GetMountMatrix("hand.R");

		// raise or lower arm based on whether or not space bar is toggled:
		mat4 rotate_arm = rotate(mat4(1), armRot, vec3(1, .3, 0));

		M = ninja_transform * right_hand * rotate(mat4(1), pi<float>() / 3.0f, vec3(1, 0, 0)) * translate(mat4(1), vec3(-.012, 0, -.011))
			* rotate(mat4(1), -pi<float>() / 4.0f, vec3(0, 1, 0)) * scale(mat4(1), vec3(.02f));
		glUniformMatrix4fv(shadowProg->getUniform("M"), 1, GL_FALSE, &M[0][0]);
		katana->draw(shadowProg, false);
		// end of drawing objects
		//done, unbind stuff
		shadowProg->unbind();

		treeShadowProg->bind();
		glUniformMatrix4fv(treeShadowProg->getUniform("P"), 1, GL_FALSE, &P[0][0]);
		glUniformMatrix4fv(treeShadowProg->getUniform("V"), 1, GL_FALSE, &V[0][0]);
		glUniform1f(treeShadowProg->getUniform("glTime"), totaltime);
		M = translate(mat4(1), vec3(13, 1, -3)) * scale(mat4(1), vec3(10));
		glUniformMatrix4fv(treeShadowProg->getUniform("M"), 1, GL_FALSE, &M[0][0]);
		tree->draw(treeShadowProg, false);

		M = translate(mat4(1), vec3(-20, 1, -3)) * scale(mat4(1), vec3(10));
		glUniformMatrix4fv(treeShadowProg->getUniform("M"), 1, GL_FALSE, &M[0][0]);
		tree->draw(treeShadowProg, false);
		treeShadowProg->unbind();
		
		glEnable(GL_BLEND);
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		/*glBindTexture(GL_TEXTURE_2D, FBOtex_shadowMapDepth);
		glGenerateMipmap(GL_TEXTURE_2D);*/
	}

	void get_compute_data()
	{
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		//copy data back to CPU MEM
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_GPU_id);
		glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(float) * 2, &ssbo_CPUMEM);
		CHECKED_GL_CALL(glUseProgram(0));

		// update current exposure value
		float fmax = (float)ssbo_CPUMEM.maxLum / (float)FFACT;
		float fmin = (float)ssbo_CPUMEM.minLum / (float)FFACT;

		exposure.x = exponential_growth(exposure.x, fmax, .6, last_frametime);
		exposure.y = exponential_growth(exposure.y, fmax, .6, last_frametime);

		// copy back over default max/min values.
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_GPU_id);
		ssbo_CPUMEM.maxLum = INT_MIN;
		ssbo_CPUMEM.minLum = INT_MAX;
		ssbo_CPUMEM.avgLum = 0;
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(ssbo_data), &ssbo_CPUMEM, GL_DYNAMIC_COPY);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}

	void blur_bloom()
	{
		// blur the output of the brighttex
		bool horizontal = true, first_iteration = true;
		int amount = 6;
		bloom_blur_prog->bind();
		glActiveTexture(GL_TEXTURE0);
		for (unsigned int i = 0; i < amount; i++)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
			glClear(GL_COLOR_BUFFER_BIT);
			glUniform1i(bloom_blur_prog->getUniform("horizontal"), horizontal);
			glBindTexture(
				GL_TEXTURE_2D, first_iteration ? colorBuffers[1] : pingpongBuffer[!horizontal]
			);
			glBindVertexArray(VertexArrayIDBox);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			horizontal = !horizontal;
			if (first_iteration)
				first_iteration = false;
		}
		bloom_blur_prog->unbind();

		glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	void render_to_bloom()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, first_bloom_pass_fbo);
		// Get current frame buffer size.

		int width, height;
		glfwGetFramebufferSize(windowManager->getHandle(), &width, &height);
		float aspect = width / (float)height;
		glViewport(0, 0, width, height);

		glm::mat4 lightP, lightV;

		get_light_proj_matrix(lightP);

		// "Camera" for rendering shadow map is at light source, looking at the scene.
		get_light_view_matrix(lightV);

		mat4 lightSpace = lightP * lightV;


		// Clear framebuffer.
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		vec3 campos;
		if (ninjaCam)
		{
			campos = mycam->pos;
		}
		else
		{
			campos = fcam.pos;
		}

		post_processing->bind();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, color_tex);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, pos_tex);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, norm_tex);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, mask_tex);
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, FBOtex_shadowMapDepth);
		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_2D, ssao->ssaoColorBufferBlur);
		glUniformMatrix4fv(post_processing->getUniform("lightSpace"), 1, GL_FALSE, &lightSpace[0][0]);
		glUniform3fv(post_processing->getUniform("camPos"), 1, &campos[0]);
		glUniform3fv(post_processing->getUniform("lightpos"), 1, &primaryLight.position.x);
		glUniform3fv(post_processing->getUniform("lightdir"), 1, &primaryLight.direction.x);
		glUniform1i(post_processing->getUniform("shadowsEnabled"), shadowsEnabled);
		glUniform1i(post_processing->getUniform("tonemappingEnabled"), tonemappingEnabled);
		glUniform2fv(post_processing->getUniform("maxmin"), 1, &exposure[0]);
		glBindVertexArray(VertexArrayIDBox);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		post_processing->unbind();

		blur_bloom();
	}

	void compute_max_min()
	{
		int width, height, channels;
		glfwGetFramebufferSize(windowManager->getHandle(), &width, &height);
		
		glUseProgram(maxMinProgram);
		glBindImageTexture(0, non_tm, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16);	// upload image as texture
		
		glDispatchCompute((GLuint)width, (GLuint)height, 1);
	}

	void render_to_screen()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClearColor(0.0, 1.0, 0.0, 1.0);
		int width, height;
		glfwGetFramebufferSize(windowManager->getHandle(), &width, &height);
		float aspect = width / (float)height;
		glViewport(0, 0, width, height);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		final_prog->bind();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
		//glBindTexture(GL_TEXTURE_2D, ssao->ssaoColorBufferBlur);	// change
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, pingpongBuffer[0]);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, mask_tex);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, view_pos_tex);
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, view_norm_tex);
		glUniformMatrix4fv(final_prog->getUniform("P"), 1, GL_FALSE, &lastUsedProj[0][0]);
		glUniform1i(final_prog->getUniform("bloomEnabled"), bloomEnabled);	// change back
		//glUniform1i(final_prog->getUniform("bloomEnabled"), 0);
		glBindVertexArray(VertexArrayIDBox);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		final_prog->unbind();
	}
};

//******************************************************************************************
int main(int argc, char **argv)
{
	std::string resourceDir = "../resources"; // Where the resources are loaded from
	std::string missingTexture = "missing.jpg";
	
	SkinnedMesh::setResourceDir(resourceDir);
	SkinnedMesh::setDefaultTexture(missingTexture);
	
	if (argc >= 2)
	{
		resourceDir = argv[1];
	}

	Application *application = new Application();

	/* your main will always include a similar set up to establish your window
		and GL context, etc. */
	WindowManager * windowManager = new WindowManager();
	windowManager->init(640, 360);
	//windowManager->init(1000, 1000);
	windowManager->setEventCallbacks(application);
	application->windowManager = windowManager;

	/* This is the code that will likely change program to program as you
		may need to initialize or set up different data and state */
	// Initialize scene.
	application->init(resourceDir);
	application->initGeom(resourceDir);
	
	int frames = 0;
	int compute = 0;	// to compute OR collect
	const int compute_rate = 4;		// number of frames it takes to collect compute shader
	// Loop until the user closes the window.
	while(! glfwWindowShouldClose(windowManager->getHandle()))
	{
		// Render scene.
		application->render_to_texture();
		application->render_ssao();
		application->render_to_shadowmap();
		if(compute == compute_rate - 1)
			application->get_compute_data();
		application->render_to_bloom();
		if(compute == 0)
			application->compute_max_min();
		// TODO: uncomment this to render scene!
		application->render_to_screen();

		static int lastTotalTime = 0;
		if(static_cast<int>(application->totaltime) != lastTotalTime)
		{
			cout << "FPS: " << (int)(frames / (application->totaltime - lastTotalTime)) << endl;
			lastTotalTime = static_cast<int>(application->totaltime);
			frames = 0;
		}

		// Swap front and back buffers.
		glfwSwapBuffers(windowManager->getHandle());
		// Poll for and process events.
		glfwPollEvents();
		frames++;
		compute = compute % compute_rate;
	}

	// Quit program.
	windowManager->shutdown();
	delete application->ssao;
	return 0;
}

