#include <algorithm>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include "Character.h"

glm::vec3 camera::get_wanted_pos()
{
	const float charSpeedInfluence = .3f;
	float distToChar = std::max<float>(4.5f, baseDistToChar * character->current_speed * charSpeedInfluence);
	
	// get idle rotation amount
	mat4 idle_rotation = mat4(1);
	if(idle_time > 5.0f)
	{
		// then we want to rotate around the character
		idle_rotation = rotate(mat4(1), -(idle_time - 5.0f) * .1f, vec3(0, 1, 0));
		distToChar += 1.0f;
	}
	
	glm::vec3 behind = glm::normalize(character->charForward) * distToChar;
	behind = idle_rotation * vec4(behind, 0);
	glm::vec3 target_pos = character->charPos - behind;
	target_pos.y += heightOffset;
	return target_pos;
}

glm::mat4 camera::process(double ftime)
{
	// rotate camera if idle for more than 5 seconds
	if(character->current_animation == 3)
	{
		idle_time += ftime;
	}
	else
	{
		// character not idle
		idle_time = 0.0f;
	}

	glm::vec3 targetcampos = get_wanted_pos();	// uses idle time to rotate around character center point (world y-axis)
	pos = exponential_growth(pos, targetcampos, .05f * 60.f, ftime);

	//return R*T;
	glm::mat4 V = glm::lookAt(pos, character->charPos + glm::vec3(0, 1.3, 0), glm::vec3(0, 1, 0));
	return V;
}