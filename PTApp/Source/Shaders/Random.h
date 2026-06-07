#ifndef RANDOM_H
#define RANDOM_H

const float PI = 3.14159265358979;

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

float RandL(float min, float max, inout uint state)
{
	return mix(min, max, Rand(state));
}

float RandF(inout uint state)
{
	return RandL(-1.0f, 1.0f, state);
}

vec3 RandS(inout uint state)
{
	float x = RandF(state);
	float y = RandF(state);
	float z = RandF(state);

	return normalize(vec3(x, y, z));
}

void BuildBasis(vec3 N, out vec3 T, out vec3 B)
{
	T = normalize(abs(N.z) < 0.999 ? cross(N, vec3(0, 0, 1)) : cross(N, vec3(0, 1, 0)));
	B = cross(T, N);
}

vec3 RandH(vec3 N, inout uint state)
{
	float r1 = Rand(state);
	float r2 = Rand(state);

	float phi = 2.0 * PI * r1;

	float x = cos(phi) * sqrt(r2);
	float y = sin(phi) * sqrt(r2);
	float z = sqrt(1.0 - r2);

	vec3 T, B;

	BuildBasis(N, T, B);

	return normalize(T * x + B * y + N * z);
}

vec3 WorldToLocal(vec3 V, vec3 N)
{
	vec3 T, B;
	BuildBasis(N, T, B);

	return vec3(dot(V, T), dot(V, B), dot(V, N));
}

vec3 LocalToWorld(vec3 V, vec3 N)
{
	vec3 T, B;
	BuildBasis(N, T, B);

	return normalize(T * V.x + B * V.y + N * V.z);
}

// Input Ve: view direction
// Input alpha_x, alpha_y: roughness parameters
// Input U1, U2: uniform random numbers
// Output Ne: normal sampled with PDF D_Ve(Ne) = G1(Ve) * max(0, dot(Ve, Ne)) * D(Ne) / Ve.z
vec3 SampleGGXVNDF(vec3 V, float alpha, inout uint state)
{
	// Section 3.2: transforming the view direction to the hemisphere configuration
	vec3 Vh = normalize(vec3(alpha * V.x, alpha * V.y, V.z));
	// Section 4.1: orthonormal basis (with special case if cross product is zero)
	float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
	vec3 T1 = lensq > 0 ? vec3(-Vh.y, Vh.x, 0) * inversesqrt(lensq) : vec3(1, 0, 0);
	vec3 T2 = cross(Vh, T1);
	// Section 4.2: parameterization of the projected area
	float r = sqrt(Rand(state));
	float phi = 2.0 * PI * Rand(state);
	float t1 = r * cos(phi);
	float t2 = r * sin(phi);
	float s = 0.5 * (1.0 + Vh.z);
	t2 = mix(sqrt(1.0 - t1 * t1), t2, s);
	// Section 4.3: reprojection onto hemisphere
	vec3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;
	// Section 3.4: transforming the normal back to the ellipsoid configuration
	vec3 Ne = normalize(vec3(alpha * Nh.x, alpha * Nh.y, max(0.0, Nh.z)));
	return Ne;
}

#endif