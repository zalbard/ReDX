/*cbuffer ConstantBuffer : register(b0) {
    float4x4 viewProj;
};*/

struct PSInput {
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

PSInput VSMain(float3 position : POSITION, float3 normal : NORMAL) {
    const float4x4 viewProj =
    {
        4.22172548e-07f, -2.97481347e-06f, -0.993231297f, -0.993230343f,
        0.000000000f, 2.61818786e-05f, -0.112936541f, -0.112936437f,
        -1.54263234e-05f, -8.14117911e-08f, -0.0271817837f, -0.0271817576f,
        0.000163963952f, -0.00571857393f, 908.843384f, 908.852539f
    };

    PSInput result;

    result.position = mul(float4(position, 1.0f), viewProj);
    result.color    = float4(abs(normal), 1.0f);

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET {
    return input.color;
}
