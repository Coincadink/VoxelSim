#pragma once

#include <vector>

struct Cube
{
	float corners[2][3];
};

struct Scene
{
	std::vector<Cube> Cubes;
};