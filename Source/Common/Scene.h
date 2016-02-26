#pragma once

#include <memory>
#include "..\D3D12\HelperStructs.h"

namespace D3D12 { class Renderer; }

// 3D scene representation
class Scene {
public:
    Scene() = delete;
    RULE_OF_ZERO(Scene);
    // Ctor; takes the .obj file name with the path as input
    // The renderer performs Direct3D resource initialization
    explicit Scene(const char* const objFilePath, D3D12::Renderer& engine);
public:
    uint                                  numObjects;
    std::unique_ptr<D3D12::IndexBuffer[]> indexBuffers;
    D3D12::VertexBuffer                   vertAttribBuffers[2]; // Positions, normals
};
