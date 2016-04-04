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

cbuffer Materials : register(b2) {
    /* TODO: remove constant */
    Material materials[26];
}

SamplerState samp       : register(s0);
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
        return textures[baseTexId].SampleLevel(samp, input.uvCoords, 0.f);
    } else {
        return float4(1.f, 0.f, 0.f, 1.f);
    }
}
