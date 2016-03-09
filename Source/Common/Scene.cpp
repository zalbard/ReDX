#include <cassert>
#include "Scene.h"
#include "Utility.h"
#include "..\Common\Camera.h"
#include "..\D3D12\Renderer.hpp"
#include "..\ThirdParty\load_obj.h"
#pragma warning(disable : 4458)
    #include "..\ThirdParty\miniball.hpp"
#pragma warning(error : 4458)

using DirectX::XMFLOAT3;
using DirectX::XMVECTOR;

using DirectX::XMVector3Dot;
using DirectX::XMVectorGetIntW;
using DirectX::XMVectorLess;
using DirectX::XMVectorNegate;
using DirectX::XMVectorSubtract;

struct IndexedPointIterator {
    const float* operator*() const {
        return reinterpret_cast<const float*>(&positions[*index]);
    }
    IndexedPointIterator& operator++() {
        ++index;
        return *this;
    }
    bool operator==(const IndexedPointIterator& other) const {
        return index == other.index;
    }
    bool operator!=(const IndexedPointIterator& other) const {
        return index != other.index;
    }
public:
    const uint*     index;
    const XMFLOAT3* positions;
};

using CoordIterator  = const float*;
using BoundingSphere = Miniball::Miniball<Miniball::CoordAccessor<IndexedPointIterator,
                                                                  CoordIterator>>;

Sphere::Sphere(DirectX::FXMVECTOR center, const float radius)
    : m_data{DirectX::XMVectorSetW(center, radius)} {}

XMVECTOR Sphere::center() const {
    return DirectX::XMVectorSetW(m_data, 0.f);
}

XMVECTOR Sphere::radius() const {
    return DirectX::XMVectorSplatW(m_data);
}

Scene::Scene(const char* const objFilePath, D3D12::Renderer& engine) {
    assert(objFilePath);
    // Load the .obj and the referenced .mtl files
    printInfo("Loading the scene from the file: %s.", objFilePath);
    obj::File objFile;
    if (!load_obj(std::string{objFilePath}, objFile)) {
        printError("Failed to load the file: %s.", objFilePath);
        TERMINATE();
    }
    uint nObj = 0;
    // Compute the total number of objects
    for (const auto& object : objFile.objects) {
        for (const auto& group : object.groups) {
            if (!group.faces.empty()) { ++nObj; }
        }
    }
    numObjects = nObj;
    // Allocate memory
    boundingSpheres      = std::make_unique<Sphere[]>(nObj);
    objectVisibilityMask = DynBitSet{nObj};
    indexBuffers         = std::make_unique<D3D12::IndexBuffer[]>(nObj);
    auto indices         = std::make_unique<std::vector<uint>[]>(nObj);
    for (uint i = 0; i < nObj; ++i) {
        indices[i].reserve(16384);
    }
    obj::IndexMap indexMap;
    indexMap.reserve(2 * objFile.vertices.size());
    // Populate vertex and index buffers
    uint objId = 0;
    for (const auto& object : objFile.objects) {
        for (const auto& group : object.groups) {
            // Test whether the group contains any polygons
            if (group.faces.empty()) { continue; }
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
                    indices[objId].insert(std::end(indices[objId]), triangle, triangle + 3);
                    prev = next;
                }
            }
            indexBuffers[objId] = engine.createIndexBuffer(static_cast<uint>(indices[objId].size()),
                                                           indices[objId].data());
            ++objId;
        }
    }
    // Create vertex attribute buffers
    const uint numVertices = static_cast<uint>(indexMap.size());
    std::vector<XMFLOAT3> positions{numVertices}, normals{numVertices};
    for (const auto& entry : indexMap) {
        positions[entry.second] = objFile.vertices[entry.first.v];
        normals[entry.second]   = objFile.normals[entry.first.n];
    }
    vertAttribBuffers[0] = engine.createVertexBuffer(numVertices, positions.data());
    vertAttribBuffers[1] = engine.createVertexBuffer(numVertices, normals.data());
    // Compute bounding spheres
    const XMFLOAT3* const positionArray = positions.data();
    for (uint i = 0; i < nObj; ++i) {
        const IndexedPointIterator begin = {indices[i].data(), positionArray};
        const IndexedPointIterator end   = {indices[i].data() + indices[i].size(), positionArray};
        const BoundingSphere sphere{3, begin, end};
        const float* const c = sphere.center();
        boundingSpheres[i] = Sphere{XMVECTOR{*c, *(c + 1), *(c + 2)},
                                    sqrt(sphere.squared_radius())};
    }
    printInfo("Scene loaded successfully.");
}

void Scene::performFrustumCulling(const PerspectiveCamera& pCam) {
    // Set all objects as visible
    objectVisibilityMask.reset(1);
    // Compute camera parameters
    const XMVECTOR camPos = pCam.position();
    const XMVECTOR camDir = pCam.computeForwardDir();
    for (uint i = 0, n = numObjects; i < n; ++i) {
        const Sphere   boundingSphere = boundingSpheres[i];
        const XMVECTOR sphereCenter   = boundingSphere.center();
        const XMVECTOR camToSphere    = XMVectorSubtract(sphereCenter, camPos);
        // Compute the signed distance from the sphere's center to the near plane
        const XMVECTOR signDist       = XMVector3Dot(camToSphere, camDir);
        // if (signDist < -boundingSphere.radius()) ...
        if (XMVectorGetIntW(XMVectorLess(signDist, XMVectorNegate(boundingSphere.radius())))) {
            // Clear the 'object visible' flag
            objectVisibilityMask.clearBit(i);
        }
    }
}
