#ifndef RANDOM_H
#define RANDOM_H

uint PCG_Hash(uint key)
{
	uint state = key * 747796405u + 2891336453u;
	uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	return (word >> 22u) ^ word;
}

float Rand(inout uint state)
{
	state = PCG_Hash(state);
	return state / 4294967295.0f;
}

float RandF(inout uint state)
{
	return Rand(state) * 2.0f - 1.0f;
}

vec3 RandS(inout uint state)
{
	float x = RandF(state);
	float y = RandF(state);
	float z = RandF(state);

	return normalize(vec3(x, y, z));
}

#endif