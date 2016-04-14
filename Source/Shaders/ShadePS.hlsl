#include "ShadeRS.hlsl"

// Contains texture array indices.
struct Material {
    uint metalTexId;        // Metallicness map index
    uint baseTexId;         // Base color texture index
    uint normalTexId;       // Normal map index
    uint maskTexId;         // Alpha mask index
    uint roughTexId;        // Roughness map index
    uint pad0, pad1, pad2;  // 16 byte alignment
};

#define MAT_CNT 32          // Maximal number of materials

cbuffer Materials : register(b0) {
    Material materials[MAT_CNT];
}

Texture2D<uint2>  depthStencilTexture : register(t0);
Texture2D<float2> normalTexture       : register(t1);
Texture2D<float2> uvCoordTexture      : register(t2);
Texture2D<float4> uvGradTexture       : register(t3);
Texture2D<uint>   materialTexture     : register(t4);
Texture2D         textures[]          : register(t5);

SamplerState      af4Sampler          : register(s0);

[RootSignature(RootSig)]
float4 main(const float4 position : SV_Position) : SV_Target {
    const int2   pixel   = int2(position.xy);
    const uint   matId   = materialTexture.Load(int3(pixel, 0));
    const float2 uvCoord = uvCoordTexture.Load(int3(pixel, 0));
    const float4 uvGrad  = uvGradTexture.Load(int3(pixel, 0));
    // Return the base color.
    const uint baseTexId = materials[matId].baseTexId;
    if (baseTexId < 0xFFFFFFFF) {
        return textures[NonUniformResourceIndex(baseTexId)].SampleGrad(af4Sampler, uvCoord,
                                                                       uvGrad.xy, uvGrad.zw);
    } else {
        return float4(1.f, 0.f, 0.f, 1.f);
    }
}
