#include <algorithm>
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
    std::vector<D3D12::Vertex> vertices;
    vertices.reserve(std::max(objFile.vertices.size(), objFile.normals.size()));
    for (const auto& vert : objFile.vertices) {
        // Insert a vertex with a specific position and a 0-initialized normal
        vertices.emplace_back(D3D12::Vertex{{vert.x, vert.y, vert.z},{}});
    }
    uint objId = 0;
    for (const auto& object : objFile.objects) {
        for (const auto& group : object.groups) {
            // Test whether the group contains any polygons
            if (group.faces.empty()) { continue; }
            indices.clear();
            for (const auto& face : group.faces) {
                if (face.index_count < 3 || face.index_count > 4) {
                    printError("Encountered a polygon with %i vertices.", face.index_count);
                    TERMINATE();
                }
                for (int i = 0; i < face.index_count; ++i) {
                    uint        vid       = face.indices[i].v;
                    auto&       vertex    = vertices[vid];
                    const auto& newNormal = objFile.normals[face.indices[i].n];
                    if (0.f == vertex.normal.x &&
                        0.f == vertex.normal.y &&
                        0.f == vertex.normal.z) {
                        // No normal has been defined for this vertex; fill it in
                        vertex.normal = newNormal;
                    } else if (newNormal.x != vertex.normal.x ||
                               newNormal.y != vertex.normal.y ||
                               newNormal.z != vertex.normal.z) {
                        // The stored normal is different; copy the vertex with a new normal
                        vid = static_cast<uint>(vertices.size());
                        vertices.emplace_back(D3D12::Vertex{vertex.position, newNormal});
                    }
                    if (3 == i) {
                        // Insert the second triangle of the quad
                        const uint last = static_cast<uint>(indices.size()) - 1;
                        const uint vid0 = indices[last - 2];
                        const uint vid2 = indices[last];
                        // Duplicate vertex indices 0 and 2
                        indices.push_back(vid0);
                        indices.push_back(vid2);
                        // Finally, insert the vertex index 3 (vid)
                    }
                    indices.push_back(vid);
                }
            }
            ibos[objId++] = engine.createIndexBuffer(static_cast<uint>(indices.size()),
                                                      indices.data());
        }
    }
    vbo = engine.createVertexBuffer(static_cast<uint>(vertices.size()), vertices.data());
    printInfo("Scene loaded successfully.");
}
