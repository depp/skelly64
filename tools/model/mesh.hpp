#pragma once

#include "tools/modelconvert/axes.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct aiMesh;
struct aiNode;
struct aiScene;
struct aiString;

namespace modelconvert {

struct Config;

std::string_view Str(const aiString &s);

class MeshError : public std::runtime_error {
public:
    MeshError(const char *msg) : runtime_error{msg} {}
    MeshError(const std::string &msg) : runtime_error{msg} {}
};

// Information about a node in the hierarchy.
struct Node {
    Node(int parent, std::string name)
        : parent{parent}, name{std::move(name)} {}

    int parent;
    std::string name;
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

// Vertex attributes, including position.
struct Vertex : VertexAttr {
    std::array<float, 3> pos;
};

// An individual triangle in a mesh.
struct Triangle {
    int material;
    std::array<int, 3> vertex;
};

// A complete mesh.
class Mesh {
    // Vertex data.
    std::vector<Vertex> m_vertex;

    // Triangles.
    std::vector<Triangle> m_triangle;

    // Bones and nodes.
    std::unordered_map<std::string, int> m_bone_names;
    std::vector<Node> m_node;

    // Quantized vertex positions.
    std::vector<std::array<int16_t, 3>> m_vertexpos;

public:
    // Import a scene as a mesh.
    static Mesh Import(const Config &cfg, std::FILE *stats,
                       const aiScene *scene);

    // Dump mesh info to file.
    void Dump(std::FILE *stats) const;

    const std::vector<Vertex> &vertex_data() const { return m_vertex; }
    const std::vector<Triangle> &triangle_data() const { return m_triangle; }
    const std::vector<std::array<int16_t, 3>> &position_data() const {
        return m_vertexpos;
    }

private:
    // Add a node and all its children recursively, given the index of the
    // parent.
    void AddNode(const aiNode *node, int parent);

    // Add a AssImp mesh.
    void AddMesh(const Config &cfg, std::FILE *stats, const aiMesh *mesh);

    // Compute static vertex positions.
    void ComputeStaticPos();
};

} // namespace modelconvert
