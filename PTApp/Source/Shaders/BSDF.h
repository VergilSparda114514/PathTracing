#ifndef BSDF_H
#define BSDF_H

const float epsilon = 0.001f;
const float PI = 3.13159265358979;

float GGX(float alpha, vec3 N, vec3 H)
{
	float alpha2 = alpha * alpha;
	float NdotH = max(dot(N, H), 0.0f);

	return alpha2 / max(PI * pow(NdotH * NdotH * (alpha2 - 1.0f) + 1.0f, 2.0f), epsilon);
}

float G1(float alpha, vec3 N, vec3 X)
{
	float k = alpha / 2.0f;
	float NdotX = max(dot(N, X), 0.0f);

	return NdotX / max((NdotX * (1.0f - k) + k), epsilon);
}

float G(float alpha, vec3 N, vec3 V, vec3 L)
{
	return G1(alpha, N, V) * G1(alpha, N, L);
}

vec3 Fresnel(vec3 F0, vec3 V, vec3 H)
{
	return F0 + (vec3(1.0f) - F0) * pow(1.0f - max(dot(V, H), 0.0f), 5.0f);
}

#endif