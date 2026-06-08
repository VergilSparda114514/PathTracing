#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "../shared_with_shaders.h"
#include "Random.h"
#include "BSDF.h"

layout(set = SCENE_SET, binding = SCENE_AS_BINDING)           uniform accelerationStructureEXT Scene;
layout(set = SCENE_SET, binding = SCENE_IMG_BINDING, rgba32f) uniform image2D ResultImage;

layout(set = SCENE_SET, binding = SCENE_LIT_BINDING, std140) uniform LitData {
	LightingParams litParams;
};

layout(set = SCENE_SET, binding = SCENE_CAM_BINDING, std430) buffer CamData {
	CameraParams camParams;
};

layout(set = MAT_SET, binding = 0, std430) readonly buffer MaterialsBuffer {
	Material Materials[];
};

layout(set = RESO_SET, binding = 0, std430) buffer ReservoirBuffer {
	Reservoir Reservoirs[];
};

layout(set = RESO_SET, binding = 1, std430) buffer PrevReservoirBuffer {
	Reservoir PrevReservoirs[];
};

layout(location = 0) rayPayloadEXT RayPayload PrimaryRay;

uint seed = (gl_LaunchIDEXT.x + gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x) * litParams.frame;

vec3 CalcRayDir(vec2 screenUV)
{
	vec3 u = camParams.camSide;
	vec3 v = camParams.camUp;

	const float planeWidth = tan(camParams.camNearFarFov.z * 0.5f);

	u *= (planeWidth * float(gl_LaunchSizeEXT.x) / float(gl_LaunchSizeEXT.y));
	v *= planeWidth;

	return normalize(camParams.camDir + (u * (screenUV.x + RandF(seed) * epsilon)) - (v * (screenUV.y + RandF(seed) * epsilon)));
}

vec3 TraceRay()
{
	const vec2 curPixel = vec2(gl_LaunchIDEXT.xy);
	const vec2 bottomRight = vec2(gl_LaunchSizeEXT.xy - 1.0f);
	const vec2 uv = (curPixel / bottomRight) * 2.0f - 1.0f;
	const bool center = vec2(gl_LaunchIDEXT.xy) == vec2(gl_LaunchSizeEXT.xy / 2.0f);

	vec3 origin = camParams.camPos;
	vec3 direction = CalcRayDir(uv);
	
	if (camParams.enableDOF)
	{
		vec3 targetDirection = origin + normalize(direction) * camParams.focalLength;
		origin += camParams.camUp * RandF(seed) * camParams.apertureSize;
		origin += camParams.camSide * RandF(seed) * camParams.apertureSize;
		direction = normalize(targetDirection - origin);
	}

	const float tMin = epsilon;
	const float tMax = camParams.camNearFarFov.y;

	vec3 incomingLight = vec3(1.0f);
	vec3 outgoingLight = vec3(0.0f);

	PrimaryRay.specular = true;

	for (int i = 0; i <= litParams.maxRecursion; i++)
	{
		PrimaryRay.bounce = i;
		traceRayEXT(Scene, gl_RayFlagsOpaqueEXT, 0xFF, 0, 1, 0, origin, tMin, direction, tMax, 0);

		const vec3 hitColor = PrimaryRay.colorAndDist.rgb;
		const float hitDistance = PrimaryRay.colorAndDist.w;

		if (i == 0 && center && camParams.autoFocus)
		{
			float targetDistance = hitDistance >= 0.0f ? hitDistance : tMax;
			camParams.focalLength = mix(camParams.focalLength, targetDistance, litParams.deltaTime * camParams.focusSpeed);
		}

		// if hit background - rage quit
		if (hitDistance < 0.0f)
		{
			outgoingLight += hitColor * incomingLight;
			break;
		}

		const vec3 hitNormal = PrimaryRay.normalAndObjID.xyz;
		const vec3 hitPos = origin + direction * PrimaryRay.colorAndDist.w;

		const Material material = Materials[PrimaryRay.matID];
		const float metallicRnd = Rand(seed);
		const float dielectricRnd = Rand(seed);

		const bool metallic = metallicRnd <= material.metallic;
		const float metallicFactor = (1.0f - material.roughness) * float(metallic);
		const vec3 diffuse = RandH(hitNormal, seed);
		
		const float alpha = material.roughness * material.roughness;
		const float cosTheta = abs(dot(-direction, hitNormal));

		const vec3 N = hitNormal;
		const vec3 V = -direction;

		const vec3 F0 = mix(vec3(pow((1.0f - material.ior) / (1.0f + material.ior), 2.0f)), material.baseReflectance, material.metallic);
		const vec3 F = F0 + (vec3(1.0) - F0) * pow(1.0 - cosTheta, 5.0);

		const float reflectProb = Luminance(F);

		if (metallicRnd <= reflectProb || dielectricRnd > material.transmittance)
		{
			PrimaryRay.specular = material.roughness < 0.1f;

			const vec3 localV = WorldToLocal(V, N);
			vec3 H = SampleGGXVNDF(localV, alpha, seed);
			H = LocalToWorld(H, N);

			const vec3 specular = reflect(direction, hitNormal);

			origin = hitPos + hitNormal * epsilon;
			direction = mix(diffuse, specular, metallicFactor);

			incomingLight *= mix(material.diffuseColor, material.specularColor, metallicFactor);
		}

		else
		{
			PrimaryRay.specular = material.smoothness > 0.9f;

			vec3 refrNormal = hitNormal;
			float refrEta = 1.0f / material.ior;

			if (dot(hitNormal, direction) > 0.0f)
			{
				refrNormal = -hitNormal;
				refrEta = material.ior;

				incomingLight *= exp(-PrimaryRay.colorAndDist.w * (1.0 - material.transmissionColor) * material.absorptionStrength);
			}

			vec3 refrDirection = refract(direction, refrNormal, refrEta);
			
			if (length(refrDirection) == 0.0f)
			{
				refrDirection = reflect(direction, hitNormal);
			}

			refrDirection = mix(diffuse, refrDirection, material.smoothness);

			origin = hitPos + refrDirection * epsilon;
			direction = refrDirection;
		}

		const float NdotL = abs(dot(hitNormal, direction));
		const float pdf = NdotL / PI;

		outgoingLight += material.emission * incomingLight;
		incomingLight *= hitColor / PI * NdotL / pdf;

		// Russian Roulette
		
		float p = max(incomingLight.r, max(incomingLight.g, incomingLight.b));
		p = clamp(p, epsilon, 1.0);
		
		if (Rand(seed) > p)
		{
		    break;
		}
		
		incomingLight /= p;
	}

	return outgoingLight;
}

void main()
{
	const ivec2 coord = ivec2(gl_LaunchIDEXT.xy);
	vec3 resultColor = vec3(0.0f);

	for (int i = 1; i <= litParams.numSamples; i++)
	{
		seed += i;
		resultColor += TraceRay();
	}
	
	resultColor = resultColor / litParams.numSamples;

	if (litParams.accumulationFrame == 0)
	{
		imageStore(ResultImage, coord, vec4(resultColor, 1.0f));
	}

	else
	{
		float t = 1.0f / float(litParams.accumulationFrame + 1);
		vec3 a = imageLoad(ResultImage, coord).rgb;
		imageStore(ResultImage, coord, vec4(mix(a, resultColor, t), 1.0f));
	}
}