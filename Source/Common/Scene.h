#pragma once

#include "DynBitSet.h"
#include "..\D3D12\HelperStructs.h"

class PerspectiveCamera;
namespace D3D12 { class Renderer; }

class Sphere {
public:
    RULE_OF_ZERO(Sphere);
    Sphere() = default;
    // Ctor; takes the center and the radius as input.
    explicit Sphere(const DirectX::XMFLOAT3& center, const float radius);
    // Returns the center of the sphere in the XYZ part; W = radius.
    DirectX::XMVECTOR center() const;
    // Returns the radius of the sphere in every component.
    DirectX::XMVECTOR radius() const;
private:
    DirectX::XMVECTOR m_data;
};

// Contains texture array indices.
struct Material {
    uint metalTexId;        // Metallicness map index
    uint baseTexId;         // Base color texture index
    uint normalTexId;       // Normal map index
    uint maskTexId;         // Alpha mask index
    uint roughTexId;        // Roughness map index
    uint pad0, pad1, pad2;  // Align to 2 * sizeof(float4)
};

// 3D scene representation.
class Scene {
public:
    RULE_OF_ZERO(Scene);
    // Ctor; takes the path and the .obj file name as input.
    // The renderer performs Direct3D resource initialization.
    explicit Scene(const char* const path, const char* const objFileName,
                   D3D12::Renderer& engine);
    // Performs culling of scene objects against the camera's frustum.
    // Returns the percentage of the visible objects.
    float performFrustumCulling(const PerspectiveCamera& pCam);
public:
    uint                                  objCount;             // Number of objects
    DynBitSet                             objVisibilityMask;    // Per-object visibility bit flags
    std::unique_ptr<Sphere[]>             boundingSpheres;      // Per-object bounding spheres
    D3D12::VertexBuffer                   vertAttribBuffers[3]; // Shared positions, normals, UVs
    std::unique_ptr<D3D12::IndexBuffer[]> indexBuffers;         // Per-object index buffers
    std::unique_ptr<uint16[]>             materialIndices;      // Per-object material indices
    uint                                  matCount;             // Number of materials
    std::unique_ptr<Material[]>           materials;            // Shared index-based materials
    uint                                  texCount;             // Number of textures
    std::unique_ptr<D3D12::Texture[]>     textures;             // Shared maps/masks/textures
};
