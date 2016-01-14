
#include <cassert>
#include "Scene.h"
#include "..\D3D12\Renderer.h"
#include "..\D3D12\HelperStructs.hpp"
#include "..\ThirdParty\stb_image.h"
#pragma warning(disable : 4706)
    #define TINYOBJLOADER_IMPLEMENTATION
    #include "..\ThirdParty\tiny_obj_loader.h"
#pragma warning(default : 4706)

Scene::Scene(const char* const objFilePath, const D3D12::Renderer& engine) {
    assert(objFilePath);
    // Compute the .mtl base path from 'objFilePath'
    const auto lastBackslash = strrchr(objFilePath, '\\');
    const auto mtlPathLen    = lastBackslash - objFilePath + 1;
    auto mtlPath = std::make_unique<char[]>(mtlPathLen + 1);
    strncpy_s(mtlPath.get(), mtlPathLen + 1, objFilePath, mtlPathLen);
    // Load the .obj and the referenced .mtl files
    printInfo("Loading the scene from the file: %s.", objFilePath);
    std::string warnMsg;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    if (!tinyobj::LoadObj(shapes, materials, warnMsg, objFilePath, mtlPath.get())) {
        printError("Failed to load the file: %s.", objFilePath);
        TERMINATE();
    } else if (!warnMsg.empty()) {
        printInfo("%s", warnMsg.c_str());
    }
    printInfo("Scene loaded successfully.");
    // Initialize Direct3D resources
    numObjects = static_cast<uint>(shapes.size());
    vbos = std::make_unique<D3D12::VertexBuffer[]>(numObjects);
    ibos = std::make_unique<D3D12::IndexBuffer[]>(numObjects);
    std::vector<D3D12::Vertex> vertices;
    for (uint i = 0; i < numObjects; ++i) {
        vertices.clear();
        const auto& mesh = shapes[i].mesh;
        for (uint j = 0, jMax = static_cast<uint>(mesh.positions.size()); j < jMax; j += 3) {
            D3D12::Vertex v{{mesh.positions[j],
                             mesh.positions[j + 1],
                             mesh.positions[j + 2]},
                            {mesh.normals[j],
                             mesh.normals[j + 1],
                             mesh.normals[j + 2]}};
            vertices.emplace_back(std::move(v));
        }
        vbos[i] = engine.createVertexBuffer(static_cast<uint>(vertices.size()),
                                            vertices.data());
        ibos[i] = engine.createIndexBuffer(static_cast<uint>(mesh.indices.size()),
                                           mesh.indices.data());
    }
}
