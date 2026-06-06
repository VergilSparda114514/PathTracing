#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "../shared_with_shaders.h"

layout(set = ATRIB_SET, binding = 0, std430) readonly buffer AttribsBuffer {
    VertexAttribute VertexAttribs[];
} AttribsArray[];

layout(set = FACES_SET, binding = 0, std430) readonly buffer FacesBuffer {
    FaceAttribute Faces[];
} FacesArray[];

layout(set = MAT_SET, binding = 0, std430) readonly buffer MaterialsBuffer {
    Material Materials[];
};
layout(set = MAT_SET, binding = 1) uniform sampler2D Textures[];

layout(set = NORM_SET, binding = 0) uniform sampler2D BumpMaps[];

layout(location = 0) rayPayloadInEXT RayPayload PrimaryRay;
                     hitAttributeEXT vec2 HitAttribs;

float BaryLerp(float a, float b, float c, vec3 barycentrics)
{
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

vec2 BaryLerp(vec2 a, vec2 b, vec2 c, vec3 barycentrics)
{
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

vec3 BaryLerp(vec3 a, vec3 b, vec3 c, vec3 barycentrics)
{
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

void main()
{
    vec3 barycentrics = vec3(1.0f - HitAttribs.x - HitAttribs.y, HitAttribs.x, HitAttribs.y);
    
    uint objId = uint(gl_InstanceCustomIndexEXT);
    uvec3 face = FacesArray[objId].Faces[gl_PrimitiveID].face;
    uint matID = FacesArray[objId].Faces[gl_PrimitiveID].matID;

    VertexAttribute v0 = AttribsArray[objId].VertexAttribs[int(face.x)];
    VertexAttribute v1 = AttribsArray[objId].VertexAttribs[int(face.y)];
    VertexAttribute v2 = AttribsArray[objId].VertexAttribs[int(face.z)];

    // interpolate our vertex attribs
    vec2 uv = BaryLerp(v0.uv, v1.uv, v2.uv, barycentrics);
    vec3 N = BaryLerp(v0.normal, v1.normal, v2.normal, barycentrics);
    
    mat3 normalMatrix = transpose(inverse(mat3(gl_ObjectToWorldEXT)));
    vec3 normal = normalize(normalMatrix * N);
    
    vec3 texel = textureLod(Textures[matID], uv, PrimaryRay.bounce).rgb;
    
    float strength = Materials[matID].bumpStrength;

    if (PrimaryRay.specular && strength != 0.0f)
    {
        vec3 T = normalize(BaryLerp(v0.tangent, v1.tangent, v2.tangent, barycentrics));
        vec3 B = normalize(cross(N, T));
        
        mat3 TBN = mat3(T, B, N);

        vec2 texSize = vec2(textureSize(BumpMaps[matID], 0));
        vec2 texel = 1.0 / texSize;
        
        float dU = texel.x;
        float dV = texel.y;
        
        float h  = textureLod(BumpMaps[matID], uv, 0).r;
        float hx = textureLod(BumpMaps[matID], uv + vec2(dU, 0.0), 0).r;
        float hy = textureLod(BumpMaps[matID], uv + vec2(0.0, dV), 0).r;

        float dHdU = (hx - h);
        float dHdV = (hy - h);

        vec3 bumpNormal = normalize(vec3(-dHdU * strength, -dHdV * strength, 1.0));
        
        vec3 mappedNormal = normalize(normalMatrix * TBN * bumpNormal);
        
        if (length(mappedNormal) == 1.0f)
        {
            normal = mappedNormal;
        }
    }

    PrimaryRay.colorAndDist = vec4(texel, gl_HitTEXT);
    PrimaryRay.normalAndObjID = vec4(normal, objId);
    PrimaryRay.matID = matID;
}