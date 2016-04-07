#include "DrawRS.hlsl"

cbuffer Transforms : register(b1) {
    float4x4 viewProj;
};

struct InputVS {
    float3 position : POSITION0;
    float3 normal   : NORMAL0;
    float2 uvCoords : TEXCOORD0;
};

struct InputPS {
    float4 position : SV_POSITION;
    float3 normal   : NORMAL1;
    float2 uvCoords : TEXCOORD1;
};

[RootSignature(RootSig)]
InputPS main(InputVS input) {
    // Hint the compiler about the structure of the matrix
    // to help with constant propagation (save 3 VALU).
    const float4x4 constViewProj = {
        viewProj[0][0], viewProj[0][1], 0.f, viewProj[0][3],
        viewProj[1][0], viewProj[1][1], 0.f, viewProj[1][3],
        viewProj[2][0], viewProj[2][1], 0.f, viewProj[2][3],
        viewProj[3][0], viewProj[3][1], 1.f, viewProj[3][3]
    };
    InputPS result;
    // Transform the vertex position into homogeneous coordinates.
    result.position = mul(float4(input.position, 1.f), constViewProj);
    // Perform perspective interpolation of the vertex normal and the UV coordinates.
    result.normal   = input.normal;
    result.uvCoords = input.uvCoords;
    return result;
}
