#pragma once

#include "tools/modelconvert/vertex.hpp"

#include <cstdio>
#include <vector>

namespace modelconvert {

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

} // namespace modelconvert
