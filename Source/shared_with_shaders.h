#ifdef __cplusplus

#pragma once

// include vec & mat types (same namings as in GLSL)
#include "framework/Common.h"
#define cpp_alignas(x) alignas(x)
#define cpp_default(x) =x

#else

#define cpp_alignas(x)
#define cpp_default(x)

#endif // __cplusplus

#define SCENE_SET 0
#define ATRIB_SET 1
#define FACES_SET 2
#define MAT_SET 3
#define NORM_SET 4
#define RESO_SET 5

#define SCENE_AS_BINDING 0
#define SCENE_ENV_BINDING 1
#define SCENE_LIT_BINDING 2
#define SCENE_CAM_BINDING 3
#define SCENE_IMG_BINDING 4

struct RayPayload
{
    vec4 colorAndDist;
    vec4 normalAndObjID;
    uint matID;
    uint bounce;
    cpp_alignas(4) bool specular;
};

struct VertexAttribute
{
    cpp_alignas (16) vec2 uv;
    cpp_alignas (16) vec3 normal;
    cpp_alignas (16) vec3 tangent;
};

struct FaceAttribute
{
    cpp_alignas (16) uvec3 face;
    uint matID;
};

struct Material
{
    float roughness;
    float metallic;
    float smoothness;
    float transmittance;
    float ior;
    float absorptionStrength;
    float bumpStrength;
    cpp_alignas(16) vec3 baseReflectance;
    cpp_alignas(16) vec3 diffuseColor;
    cpp_alignas(16) vec3 specularColor;
    cpp_alignas(16) vec3 absorptionColor;
    cpp_alignas(16) vec3 emission;
};

struct Reservoir
{
    uint sampleLightID;
    uint M;
    float samplePdf;
    float weightSum;
    cpp_alignas(16) vec3 sampleDir;
    cpp_alignas(16) vec3 sampleRadiance;
};

// packed std140
struct LightingParams
{
    int numSamples cpp_default(1);
    int maxRecursion cpp_default(5);
    uint frame;
    uint accumulationFrame;
    float deltaTime;
};

struct CameraParams
{
    // DOF
    cpp_alignas(4) bool enableDOF cpp_default(true);
    cpp_alignas(4) bool autoFocus cpp_default(true);
    float focalLength;
    float apertureSize cpp_default(0.05f);
    float focusSpeed cpp_default(5.0f);

    // Camera
    cpp_alignas(16) vec3 camPos;
    cpp_alignas(16) vec3 camDir;
    cpp_alignas(16) vec3 camUp;
    cpp_alignas(16) vec3 camSide;
    cpp_alignas(16) vec3 camNearFarFov;
};

struct PostProcessParams
{
    // Bloom

    float bloomThreshold cpp_default(0.8f);
    float bloomStrength cpp_default(1.0f);
    int kernelSize cpp_default(5);

    // Adjustments
    int toneMappingMode cpp_default(2);
    float exposure cpp_default(0.5f);
    float contrast cpp_default(1.05f);
    float saturation cpp_default(1.5f);
};