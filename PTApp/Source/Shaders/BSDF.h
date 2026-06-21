#ifndef BSDF_H
#define BSDF_H

const float epsilon = 0.001f;

float D(float alpha, vec3 N, vec3 H)
{
	float alpha2 = alpha * alpha;
	float NdotH2 = pow(max(dot(N, H), 0.0), 2.0);

	return alpha2 / max(PI * pow(NdotH2 * (alpha2 - 1.0) + 1.0, 2.0), epsilon);
}

float G1(float alpha, vec3 X, vec3 N)
{
	float k = alpha / 2.0;
	float NdotX = max(dot(N, X), 0.0);

	return NdotX / max((NdotX * (1.0 - k) + k), epsilon);
}

float G(float alpha, vec3 V, vec3 N, vec3 L)
{
	return G1(alpha, V, N) * G1(alpha, L, N);
}

float Luminance(vec3 color)
{
	return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

vec3 BRDF(vec3 V, vec3 N, vec3 L, vec3 H, vec3 F, float alpha)
{
	const float NdotV = max(dot(N, V), 0.0);
	const float NdotL = max(dot(N, L), 0.0);

	vec3 numer = D(alpha, N, H) * G(alpha, V, N, L) * F;
	float denom = 4.0 * NdotV * NdotL;

	return numer / max(denom, epsilon);
}

float GGXVNDFPDF(vec3 V, vec3 N, vec3 H, float alpha)
{
	float NdotH = max(dot(N, H), 0.0);
	float VdotH = max(dot(V, H), 0.0);

	return D(alpha, N, H) * G1(alpha, N, V) * NdotH / max(4.0 * VdotH, epsilon);
}

vec3 BTDF(vec3 V, vec3 N, vec3 L, vec3 H, vec3 F, float alpha, float etaI, float etaT)
{
	const float NdotV = abs(dot(N, V));
	const float NdotL = abs(dot(N, L));
	const float VdotH = abs(dot(V, H));
	const float LdotH = abs(dot(L, H));

	vec3 numer = D(alpha, N, H) * G(alpha, V, N, L) * (1.0 - F) * etaT * etaT * VdotH * LdotH;
	float denom = NdotV * NdotL * pow(etaI * VdotH + etaT * LdotH, 2.0);

	return numer / max(denom, epsilon);
}

float GGXBTDFPDF(vec3 V, vec3 N, vec3 L, vec3 H, float alpha, float eta)
{
	float NdotH = abs(dot(N, H));
	float VdotH = abs(dot(V, H));
	float LdotH = abs(dot(L, H));

	float denom = eta * LdotH + VdotH;

	float dwh_dwi = (eta * eta * LdotH) / max(denom * denom, epsilon);

	return D(alpha, N, H) * G1(alpha, V, N) * NdotH * dwh_dwi;
}

#endif
