#ifndef BSDF_H
#define BSDF_H

const float epsilon = 0.001f;

float D(float alpha, vec3 N, vec3 H)
{
	float alpha2 = alpha * alpha;
	float NdotH2 = pow(max(dot(N, H), 0.0), 2.0);

	return alpha2 / max(PI * pow(NdotH2 * (alpha2 - 1.0) + 1.0, 2.0), epsilon);
}

float G1(float alpha, vec3 N, vec3 X)
{
	float k = alpha / 2.0;
	float NdotX = max(dot(N, X), 0.0);

	return NdotX / max((NdotX * (1.0 - k) + k), epsilon);
}

float G(float alpha, vec3 N, vec3 V, vec3 L)
{
	return G1(alpha, N, V) * G1(alpha, N, L);
}

vec3 BRDF(vec3 V, vec3 N, vec3 L, vec3 F, float alpha)
{
	vec3 H = normalize(V + L);

	const float NdotV = max(dot(N, V), 0.0);
	const float NdotL = max(dot(N, L), 0.0);

	return D(alpha, N, H) * G(alpha, N, V, L) * F / max(4.0 * NdotV * NdotL, epsilon);
}

vec3 BTDF(vec3 V, vec3 N, vec3 L, vec3 F, float alpha)
{
	return vec3(1.0);
}

#endif