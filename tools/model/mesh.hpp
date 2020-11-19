#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <vector>

struct aiMesh;
struct aiMaterial;
struct aiString;

namespace modelconvert {

struct Gfx;

std::string_view Str(const aiString &s);

class MeshError : public std::runtime_error {
public:
    MeshError(const char *msg) : runtime_error{msg} {}
    MeshError(const std::string &msg) : runtime_error{msg} {}
};

// All data that could be associated with a vertex in one structure.
struct Vertex {
    std::array<int16_t, 3> pos;
    std::array<int16_t, 2> texcoord;
    std::array<uint8_t, 3> color;
    uint8_t alpha;
    std::array<int8_t, 3> normal;

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
    // If true, vertex normals and added to the vertex data.
    bool use_normals;
    // The amount to scale the model.
    float scale;
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

// A complete mesh.
struct Mesh {
    VertexSet vertexes;
    std::vector<Triangle> triangles;

    // Add an Assimp mesh to the mesh, giving its faces the given material
    // index.
    void AddMesh(const Config &cfg, aiMesh *mesh);

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

    // Dump mesh info to stdout.
    void Dump() const;

    // Emit as GBI commands and vertex data for the Nintendo 64. This assumes
    // that segment 1 points at the beginning of this data block.
    std::vector<uint8_t> EmitGBI(const Config &cfg,
                                 const std::vector<Material> &materials) const;
};

} // namespace modelconvert
