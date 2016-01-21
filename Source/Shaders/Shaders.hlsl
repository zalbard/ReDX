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
        -0.0277499631, -0.195538431, 9.93240246e-06, -0.993230343,
        0.000000000, 1.72096956, 1.12937562e-06, -0.112936437,
        1.01399267, -0.00535130501, 2.71820284e-07, -0.0271817576,
        -10.7775688, -375.889465, 0.0909123868, 908.852539
    };

    PSInput result;

    result.position = mul(float4(input.position, 1.0f), viewProj);
    result.color    = float4(abs(input.normal), 1.0f);

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET {
    return input.color;
}
