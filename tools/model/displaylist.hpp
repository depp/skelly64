#pragma once

#include "tools/model/gbi.hpp"
#include "tools/model/vertexcache.hpp"

#include <array>
#include <vector>

namespace modelconvert {
namespace gbi {

// Number of entries in the vertex cache.
constexpr unsigned VertexCacheSize = 32;

// Display list builder. Performs minor optimizations, such as combining
// multiple triangles into SP1Triangle and SP2Triangle.
class DisplayList {
public:
    DisplayList(unsigned cache_size, unsigned vertex_offset);

    // Read-only access to the vertex cache.
    const VertexCache &cache() const { return m_cache; }

    // The list of commands.
    const std::vector<Gfx> &command() const { return m_cmds; }

    // The list of vertexes.
    const std::vector<Vtx> &vertex() const { return m_vtx; }

    // Size of vertex cache.
    int vertex_cache_size() const { return m_cache.size(); }

    // Draw a triangle with the given vertexes, by cache index.
    void Triangle(std::array<int, 3> tri);

    // Load vertexes at the given location in cache.
    void Vertex(int offset, const std::vector<Vtx> &vertexes);

    // Modify the color of a vertex in the cache.
    void SetVertexColor(int vertex, std::array<uint8_t, 4> value);

    // Modify the texture coordinate of a vertex in the cache.
    void SetVertexTexcoord(int vertex, std::array<int16_t, 2> value);

    // End display list.
    void End();

private:
    // Flush pending triangles with the given vertex.
    void FlushVertex(int vertex);

    VertexCache m_cache;
    unsigned m_vertex_offset;

    std::vector<Gfx> m_cmds;
    std::vector<Vtx> m_vtx;

    // If the top command is SP1Triangle, then this is the triangle.
    bool m_has_tri1;
    std::array<int, 3> m_tri1;
};

} // namespace gbi
} // namespace modelconvert
