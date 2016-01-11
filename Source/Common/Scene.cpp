
#include <cassert>
#include "Scene.h"
#include "..\D3D12\Renderer.h"
#include "..\D3D12\HelperStructs.hpp"
#include "..\ThirdParty\stb_image.h"
#include "..\ThirdParty\tiny_obj_loader.h"

Scene::Scene(const char* const objFilePath, D3D12::Renderer& engine) {
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
    const auto numObjects = static_cast<uint>(shapes.size());
    objects = std::make_unique<Object[]>(numObjects);
    for (uint i = 0; i < numObjects; ++i) {
        const auto& mesh    = shapes[i].mesh;
        const auto vertices = reinterpret_cast<const D3D12::Vertex*>(mesh.positions.data());
        const auto numVerts = static_cast<uint>(mesh.positions.size() / 3);
        objects[i].vbo      = engine.createVertexBuffer(vertices, numVerts);
        objects[i].ibo      = engine.createIndexBuffer(mesh.indices.data(),
                                                       static_cast<uint>(mesh.indices.size()));
    }
}
