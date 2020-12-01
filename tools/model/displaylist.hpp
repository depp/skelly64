#pragma once

#include "tools/modelconvert/gbi.hpp"
#include "tools/modelconvert/vertexcache.hpp"

#include <array>
#include <vector>

namespace modelconvert {
namespace gbi {

// Display list builder. Performs minor optimizations, such as combining
// multiple triangles into SP1Triangle and SP2Triangle.
class DisplayList {
public:
    explicit DisplayList(unsigned cache_size);

    // Read-only access to the vertex cache.
    const VertexCache &cache() const { return m_cache; }

    // Size of vertex cache.
    int vertex_cache_size() const { return m_cache.size(); }

    // Draw a triangle with the given vertexes, by cache index.
    void Triangle(std::array<int, 3> tri);

    // Load vertexes at the given location in cache.
    void Vertex(int offset, const std::vector<Vtx> &vertexes);

    // End display list.
    void End();

    // Emit display list as model file.
    std::vector<uint8_t> Emit() const;

private:
    VertexCache m_cache;

    std::vector<Gfx> m_cmds;
    std::vector<Vtx> m_vtx;

    // If the top command is SP1Triangle, then this is the triangle.
    bool m_has_tri1;
    std::array<int, 3> m_tri1;
};

} // namespace gbi
} // namespace modelconvert
