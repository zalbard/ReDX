cbuffer cb : register(b0) {
    float4x4 viewProj;
};

struct VSInput {
    float3 position : POSITION;
    float3 normal   : NORMAL;
};

struct PSInput {
    float4 position : SV_POSITION;
    float4 color    : TEXCOORD0;
};

PSInput VSMain(VSInput input) {
    PSInput result;

    // Hint the compiler about the structure of the matrix
    // to help with constant propagation (saves 3 VALU) 
    const float4x4 constViewProj = {
        viewProj[0][0], viewProj[0][1], 0.f, viewProj[0][3],
        viewProj[1][0], viewProj[1][1], 0.f, viewProj[1][3],
        viewProj[2][0], viewProj[2][1], 0.f, viewProj[2][3],
        viewProj[3][0], viewProj[3][1], 1.f, viewProj[3][3]
    };

    result.position = mul(float4(input.position, 1.f), constViewProj);
    result.color    = float4(abs(input.normal), 1.f);

    return result;
}
