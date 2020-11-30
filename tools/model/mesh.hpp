#pragma once

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

// Axis orientation.
struct Axes {
    std::array<bool, 3> negate;
    std::array<int, 3> index;

    static Axes Default();

    template <typename T>
    std::array<T, 3> Apply(std::array<T, 3> vec) const {
        return std::array<T, 3>{{
            negate[0] ? -vec[index[0]] : vec[index[0]],
            negate[1] ? -vec[index[1]] : vec[index[1]],
            negate[2] ? -vec[index[2]] : vec[index[2]],
        }};
    }

    std::string ToString() const;
    static Axes Parse(std::string_view s);
};

std::string_view Str(const aiString &s);

class MeshError : public std::runtime_error {
public:
    MeshError(const char *msg) : runtime_error{msg} {}
    MeshError(const std::string &msg) : runtime_error{msg} {}
};

// Maximum number of bones per vertex.
constexpr size_t MaxBones = 4;

// Maxmum bone weight, the fixed-point value of 1.0.
constexpr uint32_t MaxBoneWeight = 1 << 24;

// Bone weight for a vertex.
struct BoneWeight {
    uint32_t bone;
    uint32_t weight; // 8.24, see MaxBoneWeight.

    bool operator==(const BoneWeight &w) const;
    bool operator!=(const BoneWeight &w) const;
};

// All data that could be associated with a vertex in one structure.
struct Vertex {
    std::array<int16_t, 3> pos;
    std::array<int16_t, 2> texcoord;
    std::array<uint8_t, 3> color;
    uint8_t alpha;
    std::array<int8_t, 3> normal;
    std::array<BoneWeight, MaxBones> bone_weights;

    bool operator==(const Vertex &v) const;
    bool operator!=(const Vertex &v) const;
    uint32_t Hash() const;
};

struct HashVertex {
    uint32_t operator()(const Vertex &v) const { return v.Hash(); }
};

// A set of vertexes. Deduplicates vertexes as they are added.
struct VertexSet {
    std::vector<Vertex> vertexes;
    std::unordered_map<Vertex, unsigned, HashVertex> indexes;

    // Add a vertex if it is not already present. Return its index.
    unsigned Add(const Vertex &v);
};

// An individual triangle in a mesh.
struct Triangle {
    unsigned material;
    std::array<unsigned, 3> vertex;
};

// Configuration for importing / rendering the mesh.
struct Config {
    // If true, the materials are given a primitive color equal to the
    // material’s diffuse color in the input model.
    bool use_primitive_color;
    // If true, vertex normals are added to the vertex data.
    bool use_normals;
    // If true, texture coordinates are added to the vertex data.
    bool use_texcoords;
    // If true, vertex colors are added to the vertex data.
    bool use_vertex_colors;
    // Number of fractional bits of precision for texture coordinates.
    int texcoord_bits;
    // The amount to scale the model data.
    float scale;
    // The order and sign of the axes.
    Axes axes;
    // If true, create animations.
    bool animate;
};

// Information about a material.
struct Material {
    // Primitive color.
    std::array<uint8_t, 4> rgba;

    // The default material.
    static Material Default();

    // Import a material from Assimp.
    static Material Import(const Config &cfg, aiMaterial *mat);

    // Write commands to a display list, given the previous state.
    void Write(const Material &state, std::vector<Gfx> *dl) const;

    bool operator==(const Vertex &v) const;
    bool operator!=(const Vertex &v) const;
    uint32_t Hash() const;
};

struct HashMaterial {
    uint32_t operator()(const Vertex &v) const { return v.Hash(); }
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

// A batch of triangles.
struct Batch {
    // Number of vertexes to load.
    unsigned vert_count;
    // Index of first vertex to load.
    unsigned vert_src;
    // Location in cache to load vertexes.
    unsigned vert_dest;
    // Triangles to render.
    std::vector<Triangle> triangles;
};

// A mesh which has been split into batches of primitives, each of which uses
// only enough vertexes to fill the cache.
struct BatchMesh {
    std::vector<Vertex> vertexes;
    std::vector<Batch> batches;

    // Dump mesh info to file.
    void Dump(std::FILE *stats) const;

    // Emit as GBI commands and vertex data for the Nintendo 64. This assumes
    // that segment 1 points at the beginning of this data block.
    std::vector<uint8_t> EmitGBI(const Config &cfg,
                                 const std::vector<Material> &materials) const;
};

template <typename T, std::size_t N>
struct VectorTraits {
    using Vector = std::array<T, N>;
    static Vector min_value() {
        const T value = std::numeric_limits<T>::min();
        Vector v;
        for (size_t i = 0; i < N; i++) {
            v[i] = value;
        }
        return v;
    }
    static Vector max_value() {
        const T value = std::numeric_limits<T>::max();
        Vector v;
        for (size_t i = 0; i < N; i++) {
            v[i] = value;
        }
        return v;
    }
    static Vector min(Vector x, Vector y) {
        Vector v;
        for (size_t i = 0; i < N; i++) {
            v[i] = std::min(x[i], y[i]);
        }
        return v;
    }
    static Vector max(Vector x, Vector y) {
        Vector v;
        for (size_t i = 0; i < N; i++) {
            v[i] = std::max(x[i], y[i]);
        }
        return v;
    }
};

} // namespace modelconvert
