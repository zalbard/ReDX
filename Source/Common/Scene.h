#pragma once

#include <memory>
#include "..\D3D12\HelperStructs.h"

class PerspectiveCamera;
namespace D3D12 { class Renderer; }

class Sphere {
public:
    Sphere() = default;
    Sphere(DirectX::FXMVECTOR center, const float radiusSq);
    // Returns the center of the sphere in the XYZ part; W = 0
    DirectX::XMVECTOR center() const;
    // Returns the radius of the sphere in every component
    DirectX::XMVECTOR radius() const;
private:
    DirectX::XMVECTOR m_data;
};

// 3D scene representation
class Scene {
public:
    Scene() = delete;
    RULE_OF_ZERO(Scene);
    // Ctor; takes the .obj file name with the path as input
    // The renderer performs Direct3D resource initialization
    explicit Scene(const char* const objFilePath, D3D12::Renderer& engine);
    // Performs culling of scene objects against the camera's frustum
    // Returns the number of visible objects
    uint performFrustumCulling(const PerspectiveCamera& pCam);
public:
    uint                                  numObjects;
    std::unique_ptr<Sphere[]>             boundingSpheres;
    std::unique_ptr<uint[]>               objectCullMask;  
    std::unique_ptr<D3D12::IndexBuffer[]> indexBuffers;
    D3D12::VertexBuffer                   vertAttribBuffers[2]; // Positions, normals
};
