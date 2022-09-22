#pragma once
#include "Character.h"
#include <glm/gtc/matrix_transform.hpp>
class Character;

template<typename T>
T exponential_growth(T actual, T goal, float factor, float frametime)
{
	return actual + (goal - actual) * factor * frametime;
}

class camera
{
public:
	glm::vec3 pos, rot;
	float baseDistToChar, heightOffset;	// heightOffset is the distance above the player
	int w, a, s, d;
	bool running = false;
	bool arm_raised = false;
	Character* character;

	camera(Character* c)
	{
		character = c;
		baseDistToChar = 3.5f;
		heightOffset = 1.5f;
		w = a = s = d = 0;

		// initialize the position of the camera behind the player
		pos = get_wanted_pos();
		rot = glm::vec3(0, -glm::pi<float>(), 0);
	}

	glm::vec3 get_wanted_pos();

	glm::mat4 process(double ftime);

private:
	float idle_time = 0.0f;
};