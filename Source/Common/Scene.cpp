#include <DirectXTex\DirectXTex.h>
#include <load_obj.h>
#pragma warning(disable : 4458)
#include <miniball.hpp>
#pragma warning(error : 4458)
#include "Math.h"
#include "Scene.h"
#include "Utility.h"
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

// Verifies vertex position and texture coordinates of the triangle.
// Returns 'false' in case the configuration is degenerate.
static inline auto verifyTriangleCoordinates(const XMFLOAT3(&pts)[3], const XMFLOAT2(&uvs)[3])
-> bool {
    constexpr float    eps   = 0.0001f;
    constexpr XMVECTOR vEps  = {eps, eps, eps, eps};
    constexpr XMVECTOR vOne  = {1.f, 1.f, 1.f, 1.f};
    const     XMVECTOR vTrue = XMVectorTrueInt();
    // Verify vertex position uniqueness.
    const XMVECTOR pt0 = XMLoadFloat3(&pts[0]);
    const XMVECTOR pt1 = XMLoadFloat3(&pts[1]);
    const XMVECTOR pt2 = XMLoadFloat3(&pts[2]);
    if (XMVector4EqualInt(vTrue, XMVectorNearEqual(pt0, pt1, vEps)) ||
        XMVector4EqualInt(vTrue, XMVectorNearEqual(pt1, pt2, vEps)) ||
        XMVector4EqualInt(vTrue, XMVectorNearEqual(pt0, pt2, vEps))) {
        return false;
    }
    // Verify that triangle position edges are not collinear.
    const XMVECTOR ep1 = SSE4::XMVector3Normalize(pt1 - pt0);
    const XMVECTOR ep2 = SSE4::XMVector3Normalize(pt2 - pt0);
    const XMVECTOR epD = XMVectorAbs(SSE4::XMVector3Dot(ep1, ep2));
    if (XMVectorGetX(XMVectorNearEqual(epD, vOne, vEps))) {
        return false;
    }
    // Verify vertex texture coordinate uniqueness.
    const XMVECTOR uv0 = XMLoadFloat2(&uvs[0]);
    const XMVECTOR uv1 = XMLoadFloat2(&uvs[1]);
    const XMVECTOR uv2 = XMLoadFloat2(&uvs[2]);
    if (XMVector4EqualInt(vTrue, XMVectorNearEqual(uv0, uv1, vEps)) ||
        XMVector4EqualInt(vTrue, XMVectorNearEqual(uv1, uv2, vEps)) ||
        XMVector4EqualInt(vTrue, XMVectorNearEqual(uv0, uv2, vEps))) {
        return false;
    }
    // Verify that triangle edges in texture space are not collinear.
    const XMVECTOR et1 = SSE4::XMVector2Normalize(uv1 - uv0);
    const XMVECTOR et2 = SSE4::XMVector2Normalize(uv2 - uv0);
    const XMVECTOR etD = XMVectorAbs(SSE4::XMVector2Dot(et1, et2));
    if (XMVectorGetX(XMVectorNearEqual(etD, vOne, vEps))) {
        return false;
    }
    return true;
}

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
                const auto i0 = face.indices[0];
                const uint v0 = indexMap[i0];
                auto i1 = face.indices[1];
                uint v1 = indexMap[i1];
                for (int i = 1, n = face.index_count - 1; i < n; ++i) {
                    const auto i2 = face.indices[i + 1];
                    const uint v2 = indexMap[i2];
                    // Get vertex positions and texture coordinates.
                    const XMFLOAT3 pts[] = {objFile.vertices[i0.v],
                                            objFile.vertices[i1.v],
                                            objFile.vertices[i2.v]};
                    const XMFLOAT2 uvs[] = {objFile.texcoords[i0.t],
                                            objFile.texcoords[i1.t],
                                            objFile.texcoords[i2.t]};
                    if (verifyTriangleCoordinates(pts, uvs)) {
                        const uint indices[] = {v0, v1, v2};
                        currObject->indices.insert(currObject->indices.end(), indices, indices + 3);
                    }
                    i1 = i2;
                    v1 = v2;
                }
            }
        }
    }
    /* TODO: implement mesh decimation. */
    // Remove empty objects.
    indexedObjects.erase(std::remove_if(indexedObjects.begin(), indexedObjects.end(),
                                        [](const IndexedObject& io) { return io.indices.empty(); }),
                                        indexedObjects.end());
    // Sort objects by material.
    std::sort(indexedObjects.begin(), indexedObjects.end(), std::greater<IndexedObject>());
    // Allocate memory.
    objects.count           = static_cast<uint>(indexedObjects.size());
    objects.boundingSpheres = std::make_unique<Sphere[]>(objects.count);
    objects.materialIndices = std::make_unique<uint16[]>(objects.count);
    objects.triTanFrameBuffers.allocate(objects.count);
    objects.indexBuffers.allocate(objects.count);
    vertexAttrBuffers.allocate(2);
    matCount  = static_cast<uint>(objFile.materials.size());
    materials = std::make_unique<Material[]>(matCount);
    // Create vertex attribute buffers.
    const uint numVertices = static_cast<uint>(indexMap.size());
    std::vector<XMFLOAT3> positions{numVertices};
    std::vector<XMFLOAT2> uvCoords{numVertices};
    for (const auto& entry : indexMap) {
        const uint vertId = entry.second;
        positions[vertId] = objFile.vertices[entry.first.v];
        uvCoords[vertId]  = objFile.texcoords[entry.first.t];
    }
    vertexAttrBuffers.assign(0, engine.createVertexBuffer(numVertices, positions.data()));
    vertexAttrBuffers.assign(1, engine.createVertexBuffer(numVertices, uvCoords.data()));
    // Create index buffers.
    for (uint i = 0; i < objects.count; ++i) {
        const auto& indices = indexedObjects[i].indices;
        objects.indexBuffers.assign(i, engine.createIndexBuffer(static_cast<uint>(indices.size()),
                                                                indices.data()));
    }
    for (uint i = 0; i < objects.count; ++i) {
        // Store material indices.
        objects.materialIndices[i] = static_cast<uint16>(indexedObjects[i].material);
    }
    // Compute tangent frames.
    std::vector<XMFLOAT4A> tanFrames;
    for (uint i = 0, n = objects.count; i < n; ++i) {
        const IndexedObject& object   = indexedObjects[i];
        const size_t         triCount = object.indices.size() / 3;
        tanFrames.clear();
        tanFrames.resize(triCount);
        for (size_t t = 0; t < triCount; ++t) {
            const uint     ids[3] = {object.indices[3 * t + 0],
                                     object.indices[3 * t + 1],
                                     object.indices[3 * t + 2]};
            const XMVECTOR pts[3] = {XMLoadFloat3(&positions[ids[0]]),
                                     XMLoadFloat3(&positions[ids[1]]),
                                     XMLoadFloat3(&positions[ids[2]])};
            const XMVECTOR uvs[3] = {XMLoadFloat2(&uvCoords[ids[0]]),
                                     XMLoadFloat2(&uvCoords[ids[1]]),
                                     XMLoadFloat2(&uvCoords[ids[2]])};
            // Compute an orthogonal tangent frame.
            const XMMATRIX mFrame = OrthogonalizeTangentFrame(ComputeTangentFrame(pts, uvs));
            // Encode the tangent frame as a quaternion.
            XMVECTOR qFrame = XMQuaternionRotationMatrix(mFrame);
            // Performance optimization for decoding:
            // make sure the largest component is non-negative.
            const uint cId = XMVector4MaxComponent(XMVectorAbs(qFrame));
            if (XMVectorGetByIndex(qFrame, cId) < 0.f) {
                // The negated quaternion represents the same rotation.
                qFrame = -qFrame;
            }
            XMStoreFloat4A(&tanFrames[t], qFrame);
        }
        const uint size = static_cast<uint>(triCount * sizeof(XMFLOAT4A));
        objects.triTanFrameBuffers.assign(i, engine.createStructuredBuffer(size, tanFrames.data()));
    }
    // Copy scene geometry to the GPU.
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
            // Metallicness map. TODO: get rid of constant color textures.
            materials[i].metalTexId  = acquireTexureIndex(material.map_ka);
            assert(materials[i].metalTexId != UINT_MAX);
            // Base color texture.
            materials[i].baseTexId   = acquireTexureIndex(material.map_kd);
            assert(materials[i].baseTexId != UINT_MAX);
            // Normal map.
            materials[i].normalTexId = acquireTexureIndex(material.map_bump);
            assert(materials[i].normalTexId != UINT_MAX);
            // Alpha mask. It's optional - opaque geometry doesn't need one.
            materials[i].maskTexId   = acquireTexureIndex(material.map_d);
            // Roughness map.
            materials[i].roughTexId  = acquireTexureIndex(material.map_ns);
            assert(materials[i].roughTexId != UINT_MAX);
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
