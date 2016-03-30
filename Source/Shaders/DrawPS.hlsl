struct PSInput {
    float4 position : SV_POSITION;
    float3 normal   : NORMAL1;
    float2 uvCoord  : TEXCOORD1;
};

float4 PSMain(PSInput input) : SV_TARGET {
    return float4(input.normal.x, input.uvCoord, 1.f);
}
