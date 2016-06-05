#include "GBufferRS.hlsl"

cbuffer ViewProj : register(b1) {
    float4 viewProjC0;
    float4 viewProjC1;
    float4 viewProjC3;
};

struct InputVS {
    float3 position : Position;
    float3 normal   : Normal;
    float2 uvCoord  : TexCoord;
};

struct InputPS {
    float4 position : SV_Position;
    float3 localPos : Position1;
    float3 normal   : Normal1;
    float2 uvCoord  : TexCoord1;
};

[RootSignature(RootSig)]
InputPS main(const InputVS input) {
    // Hint the compiler about the structure of the matrix
    // to help with constant propagation (save 3 VALU).
    const float4x4 constViewProj = {
        viewProjC0[0], viewProjC1[0], 0.f, viewProjC3[0],
        viewProjC0[1], viewProjC1[1], 0.f, viewProjC3[1],
        viewProjC0[2], viewProjC1[2], 0.f, viewProjC3[2],
        viewProjC0[3], viewProjC1[3], 1.f, viewProjC3[3]
    };
    InputPS result;
    result.position = mul(float4(input.position, 1.f), constViewProj);
    result.localPos = input.position;
    result.normal   = input.normal;
    result.uvCoord  = input.uvCoord;
    return result;
}
