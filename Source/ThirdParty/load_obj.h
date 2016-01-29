#ifndef LOAD_OBJ_H
#define LOAD_OBJ_H

// A modified version of the OBJ Loader provided by
// Arsène Pérard-Gayot (perard at cg.uni-saarland.de)

#include <algorithm>
#include <DirectXMath.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace obj {

using DirectX::XMFLOAT2;
using DirectX::XMFLOAT3;

class Path {
public:
    Path(const std::string& path)
        : path_(path)
    {
        std::replace(path_.begin(), path_.end(), '\\', '/');
        auto pos = path_.rfind('/');
        base_ = (pos != std::string::npos) ? path_.substr(0, pos)  : ".";
        file_ = (pos != std::string::npos) ? path_.substr(pos + 1) : path_;
    }

    const std::string& path() const { return path_; }
    const std::string& base_name() const { return base_; }
    const std::string& file_name() const { return file_; }

    std::string extension() const {
        auto pos = file_.rfind('.');
        return (pos != std::string::npos) ? file_.substr(pos + 1) : std::string();
    }

    std::string remove_extension() const {
        auto pos = file_.rfind('.');
        return (pos != std::string::npos) ? file_.substr(0, pos) : file_;
    }

    operator const std::string& () const {
        return path();
    }
private:
    std::string path_;
    std::string base_;
    std::string file_;
};

struct Index {
    int v, n, t;
};

struct Face {
    static constexpr int max_indices = 8;
    Index indices[max_indices];
    int index_count;
    int material;
};

struct Group {
    std::vector<Face> faces;
};

struct Object {
    std::vector<Group> groups;
};

struct Material {
    XMFLOAT3 ka;
    XMFLOAT3 kd;
    XMFLOAT3 ks;
    XMFLOAT3 ke;
    float    ns;
    float    ni;
    XMFLOAT3 tf;
    float    tr;
    float    d;
    int      illum;
    std::string map_ka;
    std::string map_kd;
    std::string map_ks;
    std::string map_ke;
    std::string map_bump;
    std::string map_d;
};

struct File {
    std::vector<Object>      objects;
    std::vector<XMFLOAT3>    vertices;
    std::vector<XMFLOAT3>    normals;
    std::vector<XMFLOAT2>    texcoords;
    std::vector<std::string> materials;
    std::vector<std::string> mtl_libs;
};

typedef std::unordered_map<std::string, Material> MaterialLib;

}

bool load_obj(const obj::Path&, obj::File&);
bool load_mtl(const obj::Path&, obj::MaterialLib&);

#endif // LOAD_OBJ_H
