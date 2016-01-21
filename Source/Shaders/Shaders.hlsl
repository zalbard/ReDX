//cbuffer : register(b0) {
//    float4x4 viewProj;
//};

struct VSInput {
    float3 position : POSITION;
    float3 normal   : NORMAL;
};

struct PSInput {
    float4 position : SV_POSITION;
    float4 color    : TEXCOORD0;
};

PSInput VSMain(VSInput input) {
    const float4x4 viewProj = {
        -0.0277499594, -0.195538431, 0.000000000, -0.993230343,
        0.000000000, 1.72096956, 0.000000000, -0.112936437,
        1.01399255, -0.00535130501, 0.000000000, -0.0271817576,
        -10.7775679, -375.889465, 1.00000000, 908.852539
    };

    PSInput result;

    result.position = mul(float4(input.position, 1.0f), viewProj);
    result.color    = float4(abs(input.normal), 1.0f);

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET {
    return input.color;
}
