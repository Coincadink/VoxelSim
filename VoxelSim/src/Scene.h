#pragma once

#include <vector>

struct Cube
{
	glm::vec3 pos;
	float corners[2][3];
};

struct Scene
{
	std::vector<Cube> Cubes;
};