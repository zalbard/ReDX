#include "ShadeRS.hlsl"
#include "ShaderMath.hlsl"

static const float3 radiance = float3(2.2f, 2.f, 1.8f);
static const float3 L        = normalize(float3(0.f, 0.90f, 0.42f));

struct Material {
    uint metalTexId;            // Metallicness map index
    uint baseTexId;             // Base color texture index
    uint bumpTexId;             // Bump map index
    uint maskTexId;             // Alpha mask index
    uint roughTexId;            // Roughness map index
    uint pad[3];                // 16 byte alignment
};

cbuffer RasterToViewDir : register(b0) {
    float m00, m01, m02,
          m10, m11, m12,
          m20, m21, m22;
};

// Transforms raster coordinates (x, y, 1) into the raster-to-camera direction in world space.
static const float3x3 rasterToViewDir = {
    m00, m01, m02,
    m10, m11, m12,
    m20, m21, m22
};

// Contains texture array indices.
StructuredBuffer<Material> materials : register(t0);
// G-buffer.
Texture2D<uint2>  depthStencilBuffer : register(t1);
Texture2D<float2> normalBuffer       : register(t2);
Texture2D<float2> uvCoordBuffer      : register(t3);
Texture2D<float4> uvGradBuffer       : register(t4);
Texture2D<uint>   matIdBuffer        : register(t5);
// All textures.
Texture2D         textures[]         : register(t6);

SamplerState      af4Samp            : register(s0);

[RootSignature(RootSig)]
float4 main(const float4 position : SV_Position) : SV_Target {
    // Load the pixel data from the G-buffer.
    const int2   pixel   = int2(position.xy);
    const uint   matId   = matIdBuffer.Load(int3(pixel, 0));
    if (matId == 0) return float4(radiance, 1.f);
    const float3 N       = decodeOctahedral(normalBuffer.Load(int3(pixel, 0)));
    const float2 uvCoord = uvCoordBuffer.Load(int3(pixel, 0));
    const float4 uvGrad  = uvGradBuffer.Load(int3(pixel, 0));
    // Look up the metallicness coefficient.
    uint texId = materials[matId].metalTexId;
    const float metallicness = textures[NonUniformResourceIndex(texId)].SampleGrad(af4Samp,
                               uvCoord, uvGrad.xy, uvGrad.zw).r;
    // Look up the base color.
    texId = materials[matId].baseTexId;
    const float3 baseColor = textures[NonUniformResourceIndex(texId)].SampleGrad(af4Samp,
                             uvCoord, uvGrad.xy, uvGrad.zw).rgb;
    // Evaluate the dielectric (Lambertian diffuse) part.
    const float3 diffuseReflectance = (1.f - metallicness) * baseColor;
    float3 brdf = diffuseReflectance * M_1_PI;
    // Attempt early termination.
    if (metallicness > 0.f) {
        // Look up roughness.
        texId = materials[matId].roughTexId;
        const float roughness = textures[NonUniformResourceIndex(texId)].SampleGrad(af4Samp,
                                uvCoord, uvGrad.xy, uvGrad.zw).r;
        // Evaluate the metallic (GGX) part.
        const float3 V = normalize(mul(float3(position.xy, 1.f), rasterToViewDir));
        const float3 specularReflectance = metallicness * baseColor;
        brdf += evalGGX(specularReflectance, roughness, N, L, V);
    }
    const float3 reflRadiance = radiance * brdf * saturate(dot(N, L));
    return float4(acesFilmToneMap(reflRadiance), 1.f);
}
