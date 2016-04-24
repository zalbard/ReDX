#include "ShadeRS.hlsl"

static const float  M_1_PI   = 0.318309873f;
static const float3 radiance = float3(5.f, 4.f, 3.f);
static const float3 L        = normalize(float3(0.f, 0.90f, 0.42f));

cbuffer Transforms : register(b0) {
    float3x3 viewToWorld;   // Transposed view matrix
    float    pad;           // Unused
    float    negInvResX;    // -1 / resX
    float    heightByResY;  // 2 * tan(vFoV / 2) / resY
    float    negHalfHeight; // -tan(vFoV / 2)
}

// Contains texture array indices.
struct Material {
    uint metalTexId;        // Metallicness map index
    uint baseTexId;         // Base color texture index
    uint normalTexId;       // Normal map index
    uint maskTexId;         // Alpha mask index
    uint roughTexId;        // Roughness map index
    uint pad0, pad1, pad2;  // 16 byte alignment
};

StructuredBuffer<Material> materials  : register(t0);

Texture2D<uint2>  depthStencilTexture : register(t1);
Texture2D<float2> normalTexture       : register(t2);
Texture2D<float2> uvCoordTexture      : register(t3);
Texture2D<float4> uvGradTexture       : register(t4);
Texture2D<uint>   matIdTexture        : register(t5);
Texture2D         textures[]          : register(t6);

SamplerState      af4Sampler          : register(s0);

// Computes the square of the value.
float sq(float v) {
    return v * v;
}

// Returns 1 for non-negative components, -1 otherwise.
float2 nonNegative(const float2 P) {
    return float2((P.x >= 0.f) ? 1.f : -1.f,
                  (P.y >= 0.f) ? 1.f : -1.f);
}

// Performs Octahedral Normal Vector decoding.
// Input:  2D point 'P' on a square [-1, 1] x [-1, 1].
// Output: normalized 3D vector 'N' on a sphere.
float3 decodeOctahedral(const float2 P) {
    float3 N = float3(P.xy, 1.f - abs(P.x) - abs(P.y));
    if (N.z < 0) {
        N.xy = (1.f - abs(N.yx)) * nonNegative(N.xy);
    }
    return normalize(N);
}

// Computes the world-space direction from the shaded fragment towards the camera.
float3 computeViewDir(const float2 position) {
    // Compute the pixel-to-camera-center direction in view space.
    // Dir = -(X, Y, Z), s.t. X = x / resX - 0.5, Y = (0.5 - y / resY) * h, Z = 1.
    const float3 viewSpaceDir = float3(position.x * negInvResX   + 0.5f,
                                       position.y * heightByResY + negHalfHeight,
                                       -1.f);
    // Transform it to world space.
    return normalize(mul(viewSpaceDir, viewToWorld));
}

// Evaluates Schlick's approximation of the visibility term.
float evalVisibilitySchlick(const float k, const float3 N, const float3 X) {
    const float cosNX = saturate(dot(N, X));
    return cosNX / (cosNX * (1.f - k) + k);
}

// Evaluates Schlick's approximation of the full Fresnel equations.
float3 evalFresnelSchlick(const float3 F0, const float3 L, const float3 H) {
    const float cosLH = saturate(dot(L, H));
    const float t     = 1.f - cosLH;
    const float t2    = sq(t);
    const float t5    = t * sq(t2);
    return F0 + (float3(1.f, 1.f, 1.f) - F0) * t5;
}

// Evaluates the GGX BRDF.
float3 evalGGX(const float3 specularAlbedo, const float roughness,
               const float3 N, const float3 L, const float3 V) {
    const float3 H = normalize(L + V);
    // Evaluate the NDF term.
    const float cosNH = saturate(dot(N, H));
    const float alpha = sq(roughness);
    const float D = sq(alpha) * M_1_PI / sq(sq(cosNH) * (sq(alpha) - 1) + 1);
    // Perform Disney's hotness remapping.
    const float k = 0.125f * sq(roughness + 1.f);
    // Evaluate the Smith's geometric term using Schlick's approximation.
    const float G = evalVisibilitySchlick(k, N, L) * evalVisibilitySchlick(k, N, V);
    // Evaluate the Fresnel term.
    const float3 F = evalFresnelSchlick(specularAlbedo, L, H);
    return F * (D * G);
}

[RootSignature(RootSig)]
float4 main(const float4 position : SV_Position) : SV_Target {
    // Load the pixel data from the G-buffer.
    const int2   pixel   = int2(position.xy);
    const uint   matId   = matIdTexture.Load(int3(pixel, 0));
    if (matId == 0) return float4(0.f, 0.f, 0.f, 1.f);
    const float3 N       = decodeOctahedral(normalTexture.Load(int3(pixel, 0)));
    const float2 uvCoord = uvCoordTexture.Load(int3(pixel, 0));
    const float4 uvGrad  = uvGradTexture.Load(int3(pixel, 0));
    // Look up the metallicness coefficient.
    uint texId = materials[matId].metalTexId;
    const float metallicness = textures[NonUniformResourceIndex(texId)].SampleGrad(af4Sampler,
                               uvCoord, uvGrad.xy, uvGrad.zw).r;
    // Look up the base color.
    texId = materials[matId].baseTexId;
    const float3 baseColor = textures[NonUniformResourceIndex(texId)].SampleGrad(af4Sampler,
                             uvCoord, uvGrad.xy, uvGrad.zw).rgb;
    // Evaluate the dielectric (Lambertian diffuse) part.
    const float3 diffuseAlbedo = (1.f - metallicness) * baseColor;
    float3 brdf = diffuseAlbedo * M_1_PI;
    // Attempt early termination.
    if (metallicness > 0.f) {
        // Look up roughness.
        texId = materials[matId].roughTexId;
        const float roughness = textures[NonUniformResourceIndex(texId)].SampleGrad(af4Sampler,
                                uvCoord, uvGrad.xy, uvGrad.zw).r;
        // Evaluate the metallic (GGX) part.
        const float3 V = computeViewDir(position.xy);
        const float3 specularAlbedo = metallicness * baseColor;
        brdf += evalGGX(specularAlbedo, roughness, N, L, V);
    }
    return float4(radiance * brdf * saturate(dot(N, L)), 1.f);
}
