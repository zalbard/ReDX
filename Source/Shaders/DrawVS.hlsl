cbuffer Transforms : register(b0) {
    float4x4 viewProj;
    float3x3 viewMat;
};

struct VSInput {
    float3 position : POSITION0;
    float3 normal   : NORMAL0;
    float2 uvCoord  : TEXCOORD0;
};

struct PSInput {
    float4 position : SV_POSITION;
    float3 normal   : NORMAL1;
    float2 uvCoord  : TEXCOORD1;
};

PSInput VSMain(VSInput input) {
    // Hint the compiler about the structure of the matrix
    // to help with constant propagation (save 3 VALU).
    const float4x4 constViewProj = {
        viewProj[0][0], viewProj[0][1], 0.f, viewProj[0][3],
        viewProj[1][0], viewProj[1][1], 0.f, viewProj[1][3],
        viewProj[2][0], viewProj[2][1], 0.f, viewProj[2][3],
        viewProj[3][0], viewProj[3][1], 1.f, viewProj[3][3]
    };
    PSInput result;
    // Transform (and interpolate) the vertex position into homogeneous space.
    result.position = mul(float4(input.position, 1.f), constViewProj);
    // Transform (and interpolate) the vertex normal into view space.
    result.normal   = mul(input.normal, viewMat);
    // Interpolate the UV coordinates.
    result.uvCoord  = input.uvCoord;
    return result;
}
