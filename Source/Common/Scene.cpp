#include <cassert>
#include "Scene.h"
#include "..\D3D12\HelperStructs.hpp"
#include "..\D3D12\Renderer.h"
#include "..\ThirdParty\load_obj.h"
//#include "..\ThirdParty\stb_image.h"

Scene::Scene(const char* const objFilePath, const D3D12::Renderer& engine)
    : numObjects{0} {
    assert(objFilePath);
    // Load the .obj and the referenced .mtl files
    printInfo("Loading the scene from the file: %s.", objFilePath);
    obj::File objFile;
    if (!load_obj(std::string{objFilePath}, objFile)) {
        printError("Failed to load the file: %s.", objFilePath);
        TERMINATE();
    }
    // Compute the total number of objects
    for (const auto& object : objFile.objects) {
        for (const auto& group : object.groups) {
            if (!group.faces.empty()) { ++numObjects; }
        }
    }
    // Populate vertex and index buffers
    ibos = std::make_unique<D3D12::IndexBuffer[]>(numObjects);
    std::vector<uint> indices;
    indices.reserve(16384);
    obj::IndexMap indexMap;
    indexMap.reserve(2 * objFile.vertices.size());
    uint objId = 0;
    for (const auto& object : objFile.objects) {
        for (const auto& group : object.groups) {
            // Test whether the group contains any polygons
            if (group.faces.empty()) { continue; }
            indices.clear();
            for (const auto& face : group.faces) {
                // Populate the vertex index map
                for (int i = 0; i < face.index_count; ++i) {
                    if (indexMap.find(face.indices[i]) == indexMap.end()) {
                        // Insert a new key-value pair (vertex index : position in buffer)
                        indexMap.insert(std::make_pair(face.indices[i],
                                                       static_cast<uint>(indexMap.size())));
                    }
                }
                // Create indexed triangle(s)
                const uint v0   = indexMap[face.indices[0]];
                uint       prev = indexMap[face.indices[1]];
                for (int i = 1, n = face.index_count - 1; i < n; ++i) {
                    const uint next       = indexMap[face.indices[i + 1]];
                    const uint triangle[] = {v0, prev, next};
                    indices.insert(std::end(indices), triangle, triangle + 3);
                    prev = next;
                }
            }
            ibos[objId++] = engine.createIndexBuffer(static_cast<uint>(indices.size()),
                                                     indices.data());
        }
    }
    // Create a vertex buffer
    std::vector<D3D12::Vertex> vertices{indexMap.size()};
    for (const auto& entry : indexMap) {
        const auto& pos  = objFile.vertices[entry.first.v];
        const auto& norm = objFile.normals[entry.first.n];
        vertices[entry.second] = {pos, norm};
    }
    vbo = engine.createVertexBuffer(static_cast<uint>(vertices.size()), vertices.data());
    printInfo("Scene loaded successfully.");
}
