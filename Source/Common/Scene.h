#pragma once

#include "Primitives.h"
#include "..\D3D12\HelperStructs.h"

namespace D3D12 { class Renderer; }

// Contains texture array indices.
struct Material {
    uint metalTexId;        // Metallicness map index
    uint baseTexId;         // Base color texture index
    uint normalTexId;       // Normal map index
    uint maskTexId;         // Alpha mask index
    uint roughTexId;        // Roughness map index
    uint pad0, pad1, pad2;  // 16 byte alignment
};

// 3D scene representation.
class Scene {
public:
    RULE_OF_ZERO(Scene);
    // Ctor; takes the path and the .obj file name as input.
    // The renderer performs Direct3D resource initialization.
    explicit Scene(const char* path, const char* objFileName, D3D12::Renderer& engine);
public:
    struct Objects {
        uint                      count;                // Number of objects
        std::unique_ptr<Sphere[]> boundingSpheres;
        D3D12::VertexBufferSoA    vertexAttrBuffers;    // Positions, normals, UVs
        D3D12::IndexBufferSoA     indexBuffers;
        std::unique_ptr<uint16[]> materialIndices;
    }                             objects;
    uint                          matCount;             // Number of materials
    std::unique_ptr<Material[]>   materials;
    uint                          texCount;             // Number of textures
    D3D12::TextureSoA             textures;
};
