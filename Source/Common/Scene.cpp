#include <DirectXTex\DirectXTex.h>
#include <load_obj.h>
#pragma warning(disable : 4458)
#include <miniball.hpp>
#pragma warning(error : 4458)
#include "Scene.h"
#include "Utility.h"
#include "..\Common\Camera.h"
#include "..\D3D12\Renderer.hpp"

using namespace DirectX;

struct IndexedPointIterator {
    const float* operator*() const {
        return reinterpret_cast<const float*>(&positions[*index]);
    }
    IndexedPointIterator operator++() {
        auto old = *this;
        ++index;
        return old;
    }
    bool operator==(const IndexedPointIterator& other) const {
        return index == other.index;
    }
    bool operator!=(const IndexedPointIterator& other) const {
        return index != other.index;
    }
public:
    const XMFLOAT3* positions;
    const uint*     index;
};

using CoordIterator  = const float*;
using BoundingSphere = Miniball::Miniball<Miniball::CoordAccessor<IndexedPointIterator,
                                                                  CoordIterator>>;

Sphere::Sphere(const XMFLOAT3& center, const float radius)
    : m_data{center.x, center.y, center.z, radius} {}

XMVECTOR Sphere::center() const {
    return m_data;
}

XMVECTOR Sphere::radius() const {
    return XMVectorSplatW(m_data);
}

struct IndexedObject {
    bool operator<(const IndexedObject& other) const {
        return material < other.material;
    }
public:
    int               material;
    std::vector<uint> indices;
};

// Returns 'true' if the string (path or filename) has a '.tga' extension.
static inline auto hasTgaExt(const std::string& str)
-> bool {
    const auto len = str.length();
    return len > 4 && '.' == str[len - 4] &&
                      't' == str[len - 3] &&
                      'g' == str[len - 2] &&
                      'a' == str[len - 1];
}

// Converts the string 'str' to a UTF-8 character string 'wideStr' of length up to 'wideStrLen'.
static inline void convertToUtf8(const std::string& str,
                                 wchar_t* const wideStr, const uint wideStrLen) {
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                             str.c_str(), static_cast<int>(str.length() + 1),
                             wideStr, wideStrLen)) {
        const DWORD err = GetLastError(); err;
        printError("Conversion to UTF-8 failed.");
        TERMINATE();
    }
}

Scene::Scene(const char* const path, const char* const objFileName, D3D12::Renderer& engine) {
    assert(path && objFileName);
    const std::string pathStr = path;
    // Load the .obj file.
    printInfo("Loading a scene from the file: %s", objFileName);
    obj::File objFile;
    if (!load_obj(pathStr + std::string{objFileName}, objFile)) {
        printError("Failed to load the file: %s", objFileName);
        TERMINATE();
    }
    // Populate the indexed object array and the vertex index map.
    std::vector<IndexedObject> indexedObjects;
    obj::IndexMap indexMap{2 * objFile.vertices.size()};
    for (const auto& object : objFile.objects) {
        for (const auto& group : object.groups) {
            if (group.faces.empty()) continue;
            // New group -> new object.
            indexedObjects.push_back(IndexedObject{group.faces[0].material, {}});
            IndexedObject* currObject = &indexedObjects.back();
            for (const auto& face : group.faces) {
                if (face.material != currObject->material) {
                    // New material -> new object.
                    indexedObjects.push_back(IndexedObject{face.material, {}});
                    currObject = &indexedObjects.back();
                }
                for (int i = 0; i < face.index_count; ++i) {
                    if (indexMap.find(face.indices[i]) == indexMap.end()) {
                        // Insert a new key-value pair (vertex index : position in vertex buffer).
                        indexMap.insert(std::make_pair(face.indices[i],
                                                       static_cast<uint>(indexMap.size())));
                    }
                }
                // Create indexed triangle(s).
                const uint v0   = indexMap[face.indices[0]];
                uint       prev = indexMap[face.indices[1]];
                for (int i = 1, n = face.index_count - 1; i < n; ++i) {
                    const uint next       = indexMap[face.indices[i + 1]];
                    const uint triangle[] = {v0, prev, next};
                    currObject->indices.insert(currObject->indices.end(), triangle, triangle + 3);
                    prev = next;
                }
            }
        }
    }
    // Sort objects by material.
    std::sort(indexedObjects.begin(), indexedObjects.end());
    // Allocate memory.
    const uint nObj      = static_cast<uint>(indexedObjects.size());
    numObjects           = nObj;
    objectVisibilityMask = DynBitSet{nObj};
    boundingSpheres      = std::make_unique<Sphere[]>(nObj);
    materialIndices      = std::make_unique<uint16[]>(nObj);
    indexBuffers         = std::make_unique<D3D12::IndexBuffer[]>(nObj);
    // Store material indices
    for (uint i = 0; i < nObj; ++i) {
        materialIndices[i] = static_cast<uint16>(indexedObjects[i].material);
    }
    // Create index buffers
    for (uint i = 0; i < nObj; ++i) {
        const uint count = static_cast<uint>(indexedObjects[i].indices.size());
        indexBuffers[i]  = engine.createIndexBuffer(count, indexedObjects[i].indices.data());
    }
    // Create vertex attribute buffers.
    const uint numVertices = static_cast<uint>(indexMap.size());
    std::vector<XMFLOAT3> positions{numVertices}, normals{numVertices};
    for (const auto& entry : indexMap) {
        positions[entry.second] = objFile.vertices[entry.first.v];
        normals[entry.second]   = objFile.normals[entry.first.n];
    }
    vertAttribBuffers[0] = engine.createVertexBuffer(numVertices, positions.data());
    vertAttribBuffers[1] = engine.createVertexBuffer(numVertices, normals.data());
    // Copy the scene geometry to the GPU.
    engine.executeCopyCommands();
    // Compute bounding spheres.
    const XMFLOAT3* const positionArray = positions.data();
    for (uint i = 0; i < nObj; ++i) {
        const IndexedPointIterator begin = {positionArray, indexedObjects[i].indices.data()};
        const IndexedPointIterator end   = {positionArray, indexedObjects[i].indices.data() +
                                                           indexedObjects[i].indices.size()};
        const BoundingSphere sphere{3, begin, end};
        boundingSpheres[i] = Sphere{XMFLOAT3{sphere.center()}, sqrt(sphere.squared_radius())};
    }
    // Storing textures in a map to avoid duplicates.
    std::unordered_map<std::string, D3D12::Texture> textureLib;
    // This function populates the texture library.
    auto addTextureToLib = [&textureLib, &pathStr, &engine](const std::string& name) {
        // Currently, only .tga textures are supported.
        assert(hasTgaExt(name));
        // Combine the path and the filename.
        wchar_t tgaFilePath[128];
        convertToUtf8(pathStr + name, tgaFilePath, 128);
        // Load the .tga texture.
        TexMetadata  info;
        ScratchImage img;
        CHECK_CALL(LoadFromTGAFile(tgaFilePath, &info, img),
            "Failed to load the .tga file.");
        // Perform quick verification.
        assert(1 == img.GetImageCount());
        assert(TEX_DIMENSION_TEXTURE2D == info.dimension);
        // Describe the 2D texture.
        const DXGI_SAMPLE_DESC sampleDesc = {
            /* Count */   1,            // No multi-sampling
            /* Quality */ 0             // Default sampler
        };
        const D3D12_RESOURCE_DESC textureDesc = {
            /* Dimension */        D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            /* Alignment */        0,   // Automatic
            /* Width */            info.width,
            /* Height */           static_cast<uint>(info.height),
            /* DepthOrArraySize */ static_cast<uint16>(info.depth),
            /* MipLevels */        0,   // Automatic
            /* Format */           info.format,
            /* SampleDesc */       sampleDesc,
            /* Layout */           D3D12_TEXTURE_LAYOUT_UNKNOWN,
            /* Flags */            D3D12_RESOURCE_FLAG_NONE
        };
        // Create the texture.
        const uint textureSize = static_cast<uint>(img.GetPixelsSize());
        textureLib.insert(std::make_pair(name, engine.createTexture(textureDesc, textureSize,
                                                                    img.GetPixels())));
    };
    // Load the .mtl files referenced in the .obj file.
    for (const auto& matLibFileName: objFile.mtl_libs) {
        printInfo("Loading a material library from the file: %s", matLibFileName.c_str());
        obj::MaterialLib matLib;
        if (!load_mtl(pathStr + matLibFileName, matLib)) {
            printError("Failed to load the file: %s", matLibFileName);
            TERMINATE();
        }
        // Load individual materials.
        for (const auto& entry : matLib) {
            const obj::Material& material = entry.second;
            // Currently, only glossy and specular materials are supported.
            assert(2 == material.illum);
            // Load the metallicness map (0 = dielectric, 1 = metal).
            if (!material.map_ka.empty()) {
                if (textureLib.find(material.map_ka) == textureLib.end()) {
                    addTextureToLib(material.map_ka);
                }
            }
            // Load the base color texture 
            if (!material.map_kd.empty()) {
                if (textureLib.find(material.map_kd) == textureLib.end()) {
                    addTextureToLib(material.map_kd);
                }
            }
            // Load the normal map
            if (!material.map_bump.empty()) {
                if (textureLib.find(material.map_bump) == textureLib.end()) {
                    addTextureToLib(material.map_bump);
                }
            }
            // Load the alpha mask
            if (!material.map_d.empty()) {
                if (textureLib.find(material.map_d) == textureLib.end()) {
                    addTextureToLib(material.map_d);
                }
            }
            // Load the roughness map 
            if (!material.map_ns.empty()) {
                if (textureLib.find(material.map_ns) == textureLib.end()) {
                    addTextureToLib(material.map_ns);
                }
            }
            // Copy the textures to the GPU.
            engine.executeCopyCommands();
        }
    }
    // Move the textures into the array.
    textures = std::make_unique<D3D12::Texture[]>(textureLib.size());
    uint i = 0;
    for (auto& entry : textureLib) {
        textures[i++] = std::move(entry.second);
    }
    printInfo("Scene loaded successfully.");
}

float Scene::performFrustumCulling(const PerspectiveCamera& pCam) {
    // Set all objects as visible.
    objectVisibilityMask.reset(1);
    uint visObjCnt = numObjects;
    // Compute the transposed equations of the far/left/right/top/bottom frustum planes.
    // See "Fast Extraction of Viewing Frustum Planes from the WorldView-Projection Matrix".
    XMMATRIX tfp;
    XMVECTOR farPlane;
    {
        const XMMATRIX viewProjMat = pCam.computeViewProjMatrix();
        const XMMATRIX tvp         = XMMatrixTranspose(viewProjMat);
        XMMATRIX frustumPlanes;
        // Left plane.
        frustumPlanes.r[0] = tvp.r[3] + tvp.r[0];
        // Right plane.
        frustumPlanes.r[1] = tvp.r[3] - tvp.r[0];
        // Top plane.
        frustumPlanes.r[2] = tvp.r[3] - tvp.r[1];
        // Bottom plane.
        frustumPlanes.r[3] = tvp.r[3] + tvp.r[1];
        // Far plane.
        farPlane           = tvp.r[3] - tvp.r[2];
        // Compute the inverse magnitudes.
        tfp = XMMatrixTranspose(frustumPlanes);
        const XMVECTOR magsSq  = tfp.r[0] * tfp.r[0] + tfp.r[1] * tfp.r[1] + tfp.r[2] * tfp.r[2];
        const XMVECTOR invMags = XMVectorReciprocalSqrt(magsSq);
        // Normalize the plane equations.
        frustumPlanes.r[0] *= XMVectorSplatX(invMags);
        frustumPlanes.r[1] *= XMVectorSplatY(invMags);
        frustumPlanes.r[2] *= XMVectorSplatZ(invMags);
        frustumPlanes.r[3] *= XMVectorSplatW(invMags);
        farPlane            = XMPlaneNormalize(farPlane);
        // Transpose the normalized plane equations.
        tfp = XMMatrixTranspose(frustumPlanes);
    }
    // Test each object.
    for (uint i = 0, n = numObjects; i < n; ++i) {
        const Sphere   boundingSphere  =  boundingSpheres[i];
        const XMVECTOR sphereCenter    =  boundingSphere.center();
        const XMVECTOR negSphereRadius = -boundingSphere.radius();
        // Compute the distances to the left/right/top/bottom frustum planes.
        const XMVECTOR upperPart = tfp.r[0] * XMVectorSplatX(sphereCenter) +
                                   tfp.r[1] * XMVectorSplatY(sphereCenter);
        const XMVECTOR lowerPart = tfp.r[2] * XMVectorSplatZ(sphereCenter) +
                                   tfp.r[3];
        const XMVECTOR distances = upperPart + lowerPart;
        // Test the distances against the (negated) radius of the bounding sphere.
        const XMVECTOR outsideTests = XMVectorLess(distances, negSphereRadius);
        // Check if at least one of the 'outside' tests passed.
        if (XMVector4NotEqualInt(outsideTests, XMVectorFalseInt())) {
            // Clear the 'object visible' flag.
            objectVisibilityMask.clearBit(i);
            --visObjCnt;
        } else {
            // Test whether the object is in front of the camera.
            // Our projection matrix is reversed, so we use the far plane.
            const XMVECTOR distance = SSE4::XMVector4Dot(farPlane, XMVectorSetW(sphereCenter, 1.f));
            // Test the distance against the (negated) radius of the bounding sphere.
            if (XMVectorGetIntX(XMVectorLess(distance, negSphereRadius))) {
                // Clear the 'object visible' flag.
                objectVisibilityMask.clearBit(i);
                --visObjCnt;
            }
        }
    }
    return static_cast<float>(visObjCnt * 100) / static_cast<float>(numObjects);
}
