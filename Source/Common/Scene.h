#pragma once

#include "Primitives.h"
#include "..\D3D12\HelperStructs.h"

namespace D3D12 { class Renderer; }

// Contains texture array indices.
struct Material {
    uint32_t metalTexId;    // Metallicness map index
    uint32_t baseTexId;     // Base color texture index
    uint32_t bumpTexId;     // Bump map index
    uint32_t maskTexId;     // Alpha mask index
    uint32_t roughTexId;    // Roughness map index
    byte     pad[12];       // 16 byte alignment
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
        size_t                      count;              // Number of objects
        std::unique_ptr<AABox[]>    boundingBoxes;      // Per object
        D3D12::IndexBufferSoA       indexBuffers;       // Per object
        std::unique_ptr<uint16_t[]> materialIndices;    // Per object
    }                               objects;
    D3D12::VertexBufferSoA          vertexAttrBuffers;  // Positions, normals, UV coordinates
    size_t                          matCount;           // Number of materials
    std::unique_ptr<Material[]>     materials;
    size_t                          texCount;           // Number of textures
    D3D12::TextureSoA               textures;
};
