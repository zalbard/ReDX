#include "GBufferRS.hlsl"
#include "ShaderMath.hlsl"

StructuredBuffer<float4> tanFrames : register(t0);

cbuffer MaterialId : register(b0) {
    uint matId;
}

struct InputPS {
    float4 position : SV_Position;
    float2 uvCoord  : TexCoord1;
    uint   primId   : SV_PrimitiveID;
};

struct OutputPS {
    float4 tanFrame : SV_Target0;
    float2 uvCoord  : SV_Target1;
    float4 uvGrad   : SV_Target2;
    uint   matId    : SV_Target3;
};

[RootSignature(RootSig)]
OutputPS main(const InputPS input) {
    OutputPS output;
    // Compress the tangent frame.
    output.tanFrame = packQuaternion(tanFrames[input.primId]);
    // Interpolate and wrap the UV coordinates.
    output.uvCoord  = frac(input.uvCoord);
    // Compute per-pixel UV derivatives.
    output.uvGrad   = float4(ddx_fine(input.uvCoord), ddy_fine(input.uvCoord));
    // Store the object's material index.
    output.matId    = matId;
    return output;
}
