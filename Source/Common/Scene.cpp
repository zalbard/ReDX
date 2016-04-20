#include <DirectXTex\DirectXTex.h>
#include <load_obj.h>
#pragma warning(disable : 4458)
#include <miniball.hpp>
#pragma warning(error : 4458)
#include "Scene.h"
#include "Utility.h"
#include "..\Common\Camera.h"
#include "..\Common\Math.h"
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
    bool operator>(const IndexedObject& other) const {
        return material > other.material;
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
static inline void convertToUtf8(const std::string& str, const uint wideStrLen,
                                 wchar_t* wideStr) {
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                             str.c_str(), static_cast<int>(str.length() + 1),
                             wideStr, wideStrLen)) {
        const DWORD err = GetLastError(); err;
        printError("Conversion to UTF-8 failed.");
        TERMINATE();
    }
}

// Map where Key = texture name, Value = pair (texture : texture index).
using TextureMap = std::unordered_map<std::string, std::pair<D3D12::Texture, uint>>;

Scene::Scene(const char* path, const char* objFileName, D3D12::Renderer& engine) {
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
            indexedObjects.emplace_back(IndexedObject{group.faces[0].material, {}});
            IndexedObject* currObject = &indexedObjects.back();
            for (const auto& face : group.faces) {
                if (face.material != currObject->material) {
                    // New material -> new object.
                    indexedObjects.emplace_back(IndexedObject{face.material, {}});
                    currObject = &indexedObjects.back();
                }
                for (int i = 0; i < face.index_count; ++i) {
                    if (indexMap.find(face.indices[i]) == indexMap.end()) {
                        // Insert a new Key-Value pair (vertex index : position in vertex buffer).
                        indexMap.emplace(face.indices[i], static_cast<uint>(indexMap.size()));
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
    /* TODO: implement mesh decimation. */
    // Sort objects by material.
    std::sort(indexedObjects.begin(), indexedObjects.end(), std::greater<IndexedObject>());
    // Allocate memory.
    objects.count           = static_cast<uint>(indexedObjects.size());
    objects.visibilityBits  = DynBitSet{objects.count};
    objects.boundingSpheres = std::make_unique<Sphere[]>(objects.count);
    objects.materialIndices = std::make_unique<uint16[]>(objects.count);
    objects.vertexAttrBuffers.allocate(3);
    objects.indexBuffers.allocate(objects.count);
    matCount  = static_cast<uint>(objFile.materials.size());
    materials = std::make_unique<Material[]>(matCount);
    for (uint i = 0; i < objects.count; ++i) {
        // Store material indices.
        objects.materialIndices[i] = static_cast<uint16>(indexedObjects[i].material);
    }
    // Create index buffers.
    for (uint i = 0; i < objects.count; ++i) {
        const auto& indices = indexedObjects[i].indices;
        objects.indexBuffers.assign(i, engine.createIndexBuffer(static_cast<uint>(indices.size()),
                                                                indices.data()));
    }
    // Create vertex attribute buffers.
    const uint numVertices = static_cast<uint>(indexMap.size());
    std::vector<XMFLOAT3> positions{numVertices};
    std::vector<XMFLOAT3> normals{numVertices};
    std::vector<XMFLOAT2> uvCoords{numVertices};
    for (const auto& entry : indexMap) {
        positions[entry.second] = objFile.vertices[entry.first.v];
        normals[entry.second]   = objFile.normals[entry.first.n];
        uvCoords[entry.second]  = objFile.texcoords[entry.first.t];
    }
    objects.vertexAttrBuffers.assign(0, engine.createVertexBuffer(numVertices, positions.data()));
    objects.vertexAttrBuffers.assign(1, engine.createVertexBuffer(numVertices, normals.data()));
    objects.vertexAttrBuffers.assign(2, engine.createVertexBuffer(numVertices, uvCoords.data()));
    // Copy the scene geometry to the GPU.
    engine.executeCopyCommands();
    // Compute bounding spheres.
    const XMFLOAT3* positionArray = positions.data();
    for (uint i = 0; i < objects.count; ++i) {
        const IndexedPointIterator begin = {positionArray, indexedObjects[i].indices.data()};
        const IndexedPointIterator end   = {positionArray, indexedObjects[i].indices.data() +
                                                           indexedObjects[i].indices.size()};
        const BoundingSphere sphere{3, begin, end};
        objects.boundingSpheres[i] = Sphere{XMFLOAT3{sphere.center()},
                                            sqrt(sphere.squared_radius())};
    }
    // Load the .mtl files referenced in the .obj file.
    obj::MaterialLib matLib;
    for (const auto& matLibFileName: objFile.mtl_libs) {
        printInfo("Loading a material library from the file: %s", matLibFileName.c_str());
        if (!load_mtl(pathStr + matLibFileName, matLib)) {
            printError("Failed to load the file: %s", matLibFileName);
            TERMINATE();
        }
    }
    // Store textures in a map to avoid duplicates.
    TextureMap texLib;
    // Acquires the texture index by either looking it up
    // in the texture library, or loading it from disk.
    auto acquireTexureIndex = [&texLib, &pathStr, &engine](const std::string& texName) {
        if (texName.empty()) return UINT_MAX;
        // Currently, only .tga textures are supported.
        assert(hasTgaExt(texName));
        // Check whether we have to load the texture.
        const auto texIt = texLib.find(texName);
        if (texIt != texLib.end()) {
            return texIt->second.second;
        } else {
            // Combine the path and the filename.
            wchar_t tgaFilePath[128];
            convertToUtf8(pathStr + texName, 128, tgaFilePath);
            // Load the .tga texture.
            ScratchImage tmp;
            CHECK_CALL(LoadFromTGAFile(tgaFilePath, nullptr, tmp),
                       "Failed to load the .tga file.");
            // Perform quick verification.
            assert(1 == tmp.GetImageCount());
            assert(TEX_DIMENSION_TEXTURE2D == tmp.GetMetadata().dimension);
            // Flip the image.
            ScratchImage img;
            CHECK_CALL(FlipRotate(*tmp.GetImages(), TEX_FR_FLIP_VERTICAL, img),
                       "Failed to perform a vertical image flip.");
            // Generate MIP maps.
            ScratchImage mipChain;
            CHECK_CALL(GenerateMipMaps(*img.GetImages(), TEX_FILTER_DEFAULT, 0, mipChain),
                       "Failed to generate MIP maps.");
            const TexMetadata& info = mipChain.GetMetadata();
            // Describe the 2D texture.
            const D3D12_SUBRESOURCE_FOOTPRINT footprint = {
                /* Format */   info.format,
                /* Width */    static_cast<uint>(info.width),
                /* Height */   static_cast<uint>(info.height),
                /* Depth */    static_cast<uint>(info.depth),
                /* RowPitch */ static_cast<uint>(mipChain.GetImages()->rowPitch)
            };
            // Create a texture.
            const uint mipCount = static_cast<uint>(info.mipLevels);
            const auto res = texLib.emplace(texName, engine.createTexture2D(footprint, mipCount,
                                                                            mipChain.GetPixels()));
            // Return the texture index.
            const auto texIter = res.first;
            return texIter->second.second;
        }
    };
    // Load individual materials.
    for (uint i = 0; i < matCount; ++i) {
        // Locate the material within the library.
        const auto& matName = objFile.materials[i];
        const auto  matIt   = matLib.find(matName);
        if (matIt == matLib.end()) {
            printWarning("Material '%s' (index %u) not found.", matName.c_str(), i);
            memset(&materials[i], 0xFF, sizeof(Material));
        } else {
            const obj::Material& material = matIt->second;
            // Currently, only glossy and specular materials are supported.
            assert(2 == material.illum);
            // Base color texture.
            materials[i].baseTexId   = acquireTexureIndex(material.map_kd);
            /*
                // Metallicness map.
                materials[i].metalTexId  = acquireTexureIndex(material.map_ka);
                // Normal map.
                materials[i].normalTexId = acquireTexureIndex(material.map_bump);
                // Alpha mask.
                materials[i].maskTexId   = acquireTexureIndex(material.map_d);
                // Roughness map.
                materials[i].roughTexId  = acquireTexureIndex(material.map_ns);
            */
            // Copy textures to the GPU.
            engine.executeCopyCommands();
        }

    }
    // Copy materials to the GPU.
    engine.setMaterials(matCount, materials.get());
    engine.executeCopyCommands();
    // Move textures into the array.
    texCount = static_cast<uint>(texLib.size());
    textures.allocate(texCount);
    uint i = 0;
    for (auto& entry : texLib) {
        auto& texture = entry.second;
        textures.assign(i++, std::move(texture.first));
    }
    printInfo("Scene loaded successfully.");
}

float Scene::performFrustumCulling(const PerspectiveCamera& pCam) {
    /* TODO: switch to AABBs and implement front-to-back sorting. */
    // Set all objects as visible.
    objects.visibilityBits.reset(1);
    uint visObjCnt = objects.count;
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
    for (uint i = 0, n = objects.count; i < n; ++i) {
        const Sphere   boundingSphere  =  objects.boundingSpheres[i];
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
            objects.visibilityBits.clearBit(i);
            --visObjCnt;
        } else {
            // Test whether the object is in front of the camera.
            // Our projection matrix is reversed, so we use the far plane.
            const XMVECTOR distance = SSE4::XMVector4Dot(farPlane, XMVectorSetW(sphereCenter, 1.f));
            // Test the distance against the (negated) radius of the bounding sphere.
            if (XMVectorGetIntX(XMVectorLess(distance, negSphereRadius))) {
                // Clear the 'object visible' flag.
                objects.visibilityBits.clearBit(i);
                --visObjCnt;
            }
        }
    }
    return static_cast<float>(visObjCnt) / static_cast<float>(objects.count);
}
