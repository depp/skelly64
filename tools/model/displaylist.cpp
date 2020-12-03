#include "tools/modelconvert/displaylist.hpp"

#include "tools/util/pack.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace modelconvert {
namespace gbi {

DisplayList::DisplayList(unsigned cache_size)
    : m_cache{cache_size}, m_has_tri1{false} {}

void DisplayList::Triangle(std::array<int, 3> tri) {
    for (int i = 0; i < 3; i++) {
        const int idx = tri[i];
        const Vtx *vtx = m_cache.Get(idx);
        if (vtx == nullptr) {
            throw std::runtime_error(
                "DisplayList::Triangle: vertex not loaded");
        }
        for (int j = 0; j < i; j++) {
            if (idx == tri[j]) {
                return;
                throw std::runtime_error(
                    "DisplayList::Triangle: degenerate triangle");
            }
        }
    }
    if (m_has_tri1) {
        assert(!m_cmds.empty());
        m_cmds.back() = Gfx::SP2Triangle(m_tri1, tri);
        m_has_tri1 = false;
    } else {
        m_cmds.push_back(Gfx::SP1Triangle(tri));
        m_has_tri1 = true;
        m_tri1 = tri;
    }
}

void DisplayList::Vertex(int offset, const std::vector<Vtx> &vertexes) {
    int start = offset, end = offset + vertexes.size();
    if (!(0 <= start && start <= end && end <= m_cache.size())) {
        throw std::range_error("DisplayList::Vertex: bad range");
    }
    if (start == end) {
        return;
    }
    m_cmds.push_back(Gfx::SPVertex(RSPAddress(m_vtx.size() * Vtx::Size),
                                   end - start, start));
    int pos = start;
    for (const Vtx &v : vertexes) {
        m_cache.Set(pos, v);
        m_vtx.push_back(v);
        pos++;
    }
    if (m_has_tri1) {
        bool ok = true;
        for (const int idx : m_tri1) {
            if (start <= idx && idx < end) {
                ok = false;
                break;
            }
        }
        if (ok) {
            size_t idx = m_cmds.size() - 2;
            std::swap(m_cmds.at(idx), m_cmds.at(idx + 1));
        } else {
            m_has_tri1 = false;
        }
    }
}

void DisplayList::SetVertexColor(int vertex, std::array<uint8_t, 4> value) {
    Vtx *vtx = m_cache.Get(vertex);
    if (vtx == nullptr) {
        throw std::runtime_error("cannot modify vertex, not in cache");
    }
    if (vtx->color != value) {
        vtx->color = value;
        FlushVertex(vertex);
        m_cmds.push_back(Gfx::SPModifyVertex(vertex, VertexField::RGBA,
                                             util::Pack8x4(value)));
    }
}

void DisplayList::SetVertexTexcoord(int vertex, std::array<int16_t, 2> value) {
    Vtx *vtx = m_cache.Get(vertex);
    if (vtx == nullptr) {
        throw std::runtime_error("cannot modify vertex, not in cache");
    }
    if (vtx->texcoord != value) {
        vtx->texcoord = value;
        FlushVertex(vertex);
        // HACK: We are just hard-coding the RSP scaling factor here.
        m_cmds.push_back(
            Gfx::SPModifyVertex(vertex, VertexField::ST,
                                util::Pack16x2(value[0] >> 1, value[1] >> 1)));
    }
}

void DisplayList::FlushVertex(int vertex) {
    if (m_has_tri1) {
        for (const int vertex_id : m_tri1) {
            if (vertex_id == vertex) {
                m_has_tri1 = false;
                break;
            }
        }
    }
}

void DisplayList::End() {
    m_cmds.push_back(Gfx::SPEndDisplayList());
    m_has_tri1 = false;
    m_cache.Clear();
}

} // namespace gbi
} // namespace modelconvert
