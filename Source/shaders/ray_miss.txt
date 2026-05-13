#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : require

#include "../shared_with_shaders.h"

layout(set = SCENE_SET, binding = SCENE_ENV_BINDING) uniform sampler2D EnvTexture;

layout(location = 0) rayPayloadInEXT RayPayload PrimaryRay;

const float MY_PI = 3.1415926535897932384626433832795;
const float MY_INV_PI  = 1.0 / MY_PI;

vec2 DirToLatLong(vec3 dir)
{
    float phi = atan(dir.x, dir.z);
    float theta = acos(dir.y);

    return vec2((MY_PI + phi) * MY_INV_PI * 0.5, theta * MY_INV_PI);
}

void main()
{
    vec2 uv = DirToLatLong(gl_WorldRayDirectionEXT);
    vec3 envColor = textureLod(EnvTexture, uv, 0.0f).rgb;

    PrimaryRay.colorAndDist = vec4(envColor, -1.0);
    PrimaryRay.normalAndObjID = vec4(0.0);
    PrimaryRay.matID = -1;
}