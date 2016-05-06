#include "GBufferRS.hlsl"

cbuffer MaterialId : register(b0) {
    uint matId;
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
    uint   matId    : SV_Target3;
};

// Returns 1 for non-negative components, -1 otherwise.
float2 nonNegative(const float2 P) {
    return float2((P.x >= 0.f) ? 1.f : -1.f,
                  (P.y >= 0.f) ? 1.f : -1.f);
}

// Performs Octahedral Normal Vector encoding.
// Input:  normalized 3D vector 'N' on a sphere.
// Output: 2D point 'P' on a square [-1, 1] x [-1, 1].
float2 encodeOctahedral(const float3 N) {
    // Project the sphere onto the octahedron, and then onto the XY plane.
    const float2 P = N.xy * (1.f / (abs(N.x) + abs(N.y) + abs(N.z)));
    // Reflect the folds of the lower hemisphere over the diagonals.
    return (N.z <= 0.f) ? ((1.f - abs(P.yx)) * nonNegative(P)) : P;
}

[RootSignature(RootSig)]
OutputPS main(const InputPS input) {
    OutputPS output;
    // Interpolate and encode the normal.
    output.normal  = encodeOctahedral(input.normal);
    // Interpolate and wrap the UV coordinates.
    output.uvCoord = frac(input.uvCoord);
    // Compute per-pixel UV derivatives.
    output.uvGrad  = float4(ddx_fine(input.uvCoord), ddy_fine(input.uvCoord));
    output.matId   = matId;
    return output;
}
