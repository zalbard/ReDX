#include "GBufferRS.hlsl"
#include "ShaderMath.hlsl"

Texture2D<float> bumpMap : register(t0);

SamplerState     af4Samp : register(s0);

cbuffer MaterialId : register(b0) {
    uint matId;
}

struct InputPS {
    float4 position : SV_Position;
    float3 localPos : Position1;
    float3 normal   : Normal1;
    float2 uvCoord  : TexCoord1;
};

struct OutputPS {
    float2 normal   : SV_Target0;
    float2 uvCoord  : SV_Target1;
    float4 uvGrad   : SV_Target2;
    uint   matId    : SV_Target3;
};

[RootSignature(RootSig)]
OutputPS main(const InputPS input) {
    OutputPS output;
    // Interpolate and wrap the UV coordinates.
    output.uvCoord = frac(input.uvCoord);
    // Compute per-pixel UV derivatives.
    output.uvGrad  = float4(ddx_fine(input.uvCoord), ddy_fine(input.uvCoord));
    // Store the object's material index.
    output.matId   = matId & 0x0000FFFF;
    // Interpolate and normalize the input normal.
    float3 normal  = normalize(input.normal);
    // Check whether the bump map flag is raised.
    if (matId & 0x80000000) {
        // Sample the bump map.
        const float height = bumpMap.SampleGrad(af4Samp, output.uvCoord,
                                                output.uvGrad.xy, output.uvGrad.zw);
        // Apply the bump map.
        normal = perturbNormal(normal, input.localPos, height);
    }
    // Encode and store the normal.
    output.normal = encodeOctahedral(normal);
    return output;
}
