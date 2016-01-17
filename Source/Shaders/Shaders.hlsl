//cbuffer : register(b0) {
//    float4x4 viewProj;
//};

struct PSInput {
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

PSInput VSMain(float3 position : POSITION, float3 normal : NORMAL) {
    const float4x4 viewProj =
    {
        -0.0277499631f, -0.195538431f, -0.993240297f, -0.993230343f,
        0.000000000f, 1.72096956f, -0.112937570f, -0.112936437f,
        1.01399267f, -0.00535130501f, -0.0271820296f, -0.0271817576f,
        -10.7775688f, -375.889465f, 908.761658f, 908.852539f
    };

    PSInput result;

    result.position = mul(float4(position, 1.0f), viewProj);
    result.color    = float4(abs(normal), 1.0f);

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET {
    return input.color;
}
