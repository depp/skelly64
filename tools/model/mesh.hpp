#pragma once

#include "tools/modelconvert/axes.hpp"
#include "tools/modelconvert/vertex.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct aiMaterial;
struct aiMesh;
struct aiNode;
struct aiString;

namespace modelconvert {

struct Gfx;

std::string_view Str(const aiString &s);

class MeshError : public std::runtime_error {
public:
    MeshError(const char *msg) : runtime_error{msg} {}
    MeshError(const std::string &msg) : runtime_error{msg} {}
};

// A set of vertexes. Deduplicates vertexes as they are added.
struct VertexSet {
    std::vector<FVertex> vertexes;
    std::unordered_map<Vertex, unsigned, HashVertex> indexes;

    // Add a vertex if it is not already present. Return its index.
    unsigned Add(const FVertex &v);
};

struct BatchMesh;

// Information about a node in the hierarchy.
struct Node {
    Node(int parent, std::string name)
        : parent{parent}, name{std::move(name)} {}

    int parent;
    std::string name;
};

// A complete mesh.
struct Mesh {
    VertexSet vertexes;
    std::vector<Triangle> triangles;
    std::unordered_map<std::string, uint32_t> bone_names;
    std::vector<Node> nodes;
    std::array<int16_t, 3> bounds_min, bounds_max;

    // Recursively add nodes from the given hierarchy.
    void AddNodes(const Config &cfg, std::FILE *stats, aiNode *node);

    // Add an Assimp mesh to the mesh, giving its faces the given material
    // index.
    void AddMesh(const Config &cfg, std::FILE *stats, aiMesh *mesh);

    // Convert the mesh into batches of triangle, where each batch fits
    BatchMesh MakeBatches(unsigned cache_size);
};

} // namespace modelconvert
