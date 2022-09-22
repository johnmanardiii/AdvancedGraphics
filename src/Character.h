#pragma once
#include <memory>
#include <glm/vec3.hpp>
#include "camera.h"
#include "skmesh.h"

class camera;
class SkinnedMesh;

class Character
{
public:
	glm::vec3 charPos, charForward;
	glm::vec3 baseForward = glm::vec3(0, 0, 1);
	std::shared_ptr<camera> cam;
	int current_animation;
	
	float max_run_speed = 6.0f;
	float max_walk_speed = 2.0f;
	float current_speed = 0.0f;
	
	float yrot = 0.0f;
	float angleVelocity = 90.0f;
	float currentAngleVelocity = 0.0f;
	Character()
	{
		charPos = glm::vec3(0, -1.55, 35);
		charForward = baseForward;
	}

	int update(float deltatime, SkinnedMesh& ninjaMesh);
};

