#include "GBufferRS.hlsl"

cbuffer RootConsts : register(b0) {
    uint material;
}

struct InputPS {
    float4 position : SV_Position;
    float3 normal   : Normal1;
    float2 uvCoord  : TexCoord1;
};

struct OutputPS {
    float2 normal   : SV_Target0;
    float2 uvCoord  : SV_Target1;
    float4 uvGrad   : SV_Target2;
    uint   material : SV_Target3;
};

[RootSignature(RootSig)]
OutputPS main(const InputPS input) {
    OutputPS output;
    // Normalize the interpolated normal.
    output.normal   = input.normal.xy * rsqrt(dot(input.normal, input.normal));
    // Wrap the UV coordinates.
    output.uvCoord  = frac(input.uvCoord);
    // Compute per-pixel UV derivatives.
    output.uvGrad   = float4(ddx_fine(input.uvCoord), ddy_fine(input.uvCoord));
    output.material = material;
    return output;
}
