#pragma once

#include <memory>
#include "DynBitSet.h"
#include "..\D3D12\HelperStructs.h"

class PerspectiveCamera;
namespace D3D12 { class Renderer; }

class Sphere {
public:
    RULE_OF_ZERO(Sphere);
    Sphere() = default;
    // Ctor; takes the center and the radius as input
    explicit Sphere(const DirectX::XMFLOAT3& center, const float radius);
    // Returns the center of the sphere in the XYZ part; W = radius
    DirectX::XMVECTOR center() const;
    // Returns the radius of the sphere in every component
    DirectX::XMVECTOR radius() const;
private:
    DirectX::XMVECTOR m_data;
};

// 3D scene representation
class Scene {
public:
    RULE_OF_ZERO(Scene);
    // Ctor; takes the .obj file name with the path as input
    // The renderer performs Direct3D resource initialization
    explicit Scene(const char* const path, const char* const objFileName,
                   D3D12::Renderer& engine);
    // Performs culling of scene objects against the camera's frustum
    // Returns the percentage of the visible objects
    float performFrustumCulling(const PerspectiveCamera& pCam);
public:
    uint                                  numObjects;
    std::unique_ptr<Sphere[]>             boundingSpheres;
    DynBitSet                             objectVisibilityMask;  
    std::unique_ptr<D3D12::IndexBuffer[]> indexBuffers;
    D3D12::VertexBuffer                   vertAttribBuffers[2]; // Positions, normals
};
