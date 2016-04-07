#include "DrawRS.hlsl"

cbuffer RootConsts : register(b0) {
    uint matId;
}

// Contains texture array indices.
struct Material {
    uint metalTexId;        // Metallicness map index
    uint baseTexId;         // Base color texture index
    uint normalTexId;       // Normal map index
    uint maskTexId;         // Alpha mask index
    uint roughTexId;        // Roughness map index
    uint pad0, pad1, pad2;  // Align to 2 * sizeof(float4)
};

#define MAT_CNT 32          // Maximal number of materials

cbuffer Materials : register(b2) {
    Material materials[MAT_CNT];
}

SamplerState af4Sampler : register(s0);
Texture2D    textures[] : register(t0);

struct InputPS {
    float4 position : SV_POSITION;
    float3 normal   : NORMAL1;
    float2 uvCoords : TEXCOORD1;
};

[RootSignature(RootSig)]
float4 main(InputPS input) : SV_TARGET {
    const uint baseTexId = materials[matId].baseTexId;
    if (baseTexId < 0xFFFFFFFF) {
        return textures[baseTexId].Sample(af4Sampler, input.uvCoords);
    } else {
        return float4(1.f, 0.f, 0.f, 1.f);
    }
}
