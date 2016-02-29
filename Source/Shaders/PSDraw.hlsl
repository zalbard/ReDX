struct PSInput {
    float4 position : SV_POSITION;
    float4 color    : TEXCOORD0;
};

float4 PSMain(PSInput input) : SV_TARGET {
    const float d = pow(abs(1.0f - input.position.z), 512);
    return float4(input.color.rgb * (1.f - d), 1.f);
}
