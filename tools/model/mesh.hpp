#pragma once

#include "tools/modelconvert/axes.hpp"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

struct aiScene;

namespace modelconvert {

struct Config;

class MeshError : public std::runtime_error {
public:
    MeshError(const char *msg) : runtime_error{msg} {}
    MeshError(const std::string &msg) : runtime_error{msg} {}
};

// Vertex attributes, other than position.
struct VertexAttr {
    std::array<int16_t, 2> texcoord;
    std::array<uint8_t, 4> color;
    std::array<int8_t, 3> normal;

    bool operator==(const VertexAttr &other) const {
        return texcoord == other.texcoord && color == other.color &&
               normal == other.normal;
    }

    bool operator!=(const VertexAttr &other) const { return !(*this == other); }
};

// An individual triangle in a mesh.
struct Triangle {
    int material;
    std::array<int, 3> vertex;
};

// A complete mesh.
struct Mesh {
    // Vertex attributes, excluding position.
    std::vector<VertexAttr> vertex;

    // Vertex position.
    std::vector<std::array<int16_t, 3>> vertexpos;

    // Triangles.
    std::vector<Triangle> triangle;

    // Import a scene as a mesh.
    static Mesh Import(const Config &cfg, std::FILE *stats,
                       const aiScene *scene);
};

} // namespace modelconvert
