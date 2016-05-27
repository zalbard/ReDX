#include "ShadeRS.hlsl"
#include "ShaderMath.hlsl"

static const float3 radiance = float3(2.2f, 2.f, 1.8f);
static const float3 L        = normalize(float3(0.f, 0.90f, 0.42f));

cbuffer Transforms : register(b0) {
    float3x3 rasterToWorldDir;  // Transforms the raster coordinates (x, y, 1) into
    float    pad;               // the raster-to-camera-center direction in world space.
}

// Contains texture array indices.
struct Material {
    uint metalTexId;            // Metallicness map index
    uint baseTexId;             // Base color texture index
    uint normalTexId;           // Normal map index
    uint maskTexId;             // Alpha mask index
    uint roughTexId;            // Roughness map index
    uint pad0, pad1, pad2;      // 16 byte alignment
};

StructuredBuffer<Material> materials  : register(t0);

Texture2D<uint2>  depthStencilTexture : register(t1);
Texture2D<float4> tanFrameTexture     : register(t2);
Texture2D<float2> uvCoordTexture      : register(t3);
Texture2D<float4> uvGradTexture       : register(t4);
Texture2D<uint>   matIdTexture        : register(t5);
Texture2D         textures[]          : register(t6);

SamplerState      af4Sampler          : register(s0);

[RootSignature(RootSig)]
float4 main(const float4 position : SV_Position) : SV_Target {
    // Load the pixel data from the G-buffer.
    const int2   pixel   = int2(position.xy);
    const uint   matId   = matIdTexture.Load(int3(pixel, 0));
    if (matId == 0) return float4(radiance, 1.f);
    const float4 qFrame  = unpackQuaternion(tanFrameTexture.Load(int3(pixel, 0)));
    const float2 uvCoord = uvCoordTexture.Load(int3(pixel, 0));
    const float4 uvGrad  = uvGradTexture.Load(int3(pixel, 0));
    // Look up the normal from the map.
    uint texId = materials[matId].normalTexId;
    const float3 localN = textures[NonUniformResourceIndex(texId)].SampleGrad(af4Sampler,
                          uvCoord, uvGrad.xy, uvGrad.zw).rgb;
    // Decode the tangent frame quaternion representation.
    const float3x3 tanFrame = convertQuaternionToTangentFrame(qFrame);
    // Apply the normal map.
    const float3 N = mul(float3(0, 1, 0), tanFrame);
    // Look up the metallicness coefficient.
    texId = materials[matId].metalTexId;
    const float metallicness = textures[NonUniformResourceIndex(texId)].SampleGrad(af4Sampler,
                               uvCoord, uvGrad.xy, uvGrad.zw).r;
    // Look up the base color.
    texId = materials[matId].baseTexId;
    const float3 baseColor = textures[NonUniformResourceIndex(texId)].SampleGrad(af4Sampler,
                             uvCoord, uvGrad.xy, uvGrad.zw).rgb;
    // Evaluate the dielectric (Lambertian diffuse) part.
    const float3 diffuseReflectance = (1.f - metallicness) * baseColor;
    float3 brdf = diffuseReflectance * M_1_PI;
    // Attempt early termination.
    if (metallicness > 0.f) {
        // Look up roughness.
        texId = materials[matId].roughTexId;
        const float roughness = textures[NonUniformResourceIndex(texId)].SampleGrad(af4Sampler,
                                uvCoord, uvGrad.xy, uvGrad.zw).r;
        // Evaluate the metallic (GGX) part.
        const float3 V = normalize(mul(float3(position.xy, 1.f), rasterToWorldDir));
        const float3 specularReflectance = metallicness * baseColor;
        brdf += evalGGX(specularReflectance, roughness, N, L, V);
    }
    const float3 reflRadiance = radiance * brdf * saturate(dot(N, L));
    return float4(acesFilmToneMap(reflRadiance), 1.f);
}
