#include "camera.h"
#include "Character.h"
#include "skmesh.h"

/*
 * update returns the index of the currently running animation based on input data
 * 1 = run
 * 2 = walk
 * 3 = idle
 *
 * this method handles the direction, input, and speed of the character.
 * speed should be incorperated into the animation when the player starts/stops walking so that it might look natural idk
 */
int Character::update(float deltatime, SkinnedMesh &ninjaMesh)
{
	float ychange = 0;
	if (cam->a)
	{
		ychange = angleVelocity;
	}
	if (cam->d)
	{
		ychange = -angleVelocity;
	}

	float goal = ychange;
	if(!cam->a && !cam->d)
	{
		goal = 0.0f;	// slow down turning
	}

	currentAngleVelocity = exponential_growth(currentAngleVelocity, goal, .9f * 60.0f, deltatime);

	yrot += currentAngleVelocity * deltatime;
	charForward = glm::rotate(glm::mat4(1), glm::radians(yrot), glm::vec3(0, 1, 0)) *
		glm::vec4(baseForward, 1);

	float speed = 0.0f;
	int animation_state = 3;
	if(cam->w)
	{
		speed = max_walk_speed;
		animation_state = 2;
	}
	if(cam->s && !cam->w)
	{
		speed = -max_walk_speed;
		animation_state = 2;
	}
	else if(cam->w && cam->running)
	{
		speed = max_run_speed;
		animation_state = 1;
	}
	current_speed = exponential_growth(current_speed, speed, .05f * 60.0f, deltatime);
	ninjaMesh.turnInPlace = false;
	if(!cam->w && ychange != 0)
	{
		// turning in place animation
		animation_state = 2;
		ninjaMesh.turnInPlace = true;
	}
	
	charPos += charForward * deltatime * current_speed;		// TODO: add speed into this equation as to how far it goes forward.
	current_animation = animation_state;
	return animation_state;
}
