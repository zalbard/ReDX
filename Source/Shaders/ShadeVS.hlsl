#include "ShadeRS.hlsl"

[RootSignature(RootSig)]
float4 main(const uint vertexId : SV_VertexID) : SV_Position {
    // Generate a triangle in homogeneous clip space, s.t.
    // v0 = (-1, -1, 0), v1 = (3, -1, 0), v2 = (-1, 3, 0).
    return float4(float(vertexId / 2) * 4.f - 1.f,
                  float(vertexId % 2) * 4.f - 1.f,
                  0.f, 1.f);
}
