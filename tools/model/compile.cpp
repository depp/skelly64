#include "tools/modelconvert/compile.hpp"

#include "tools/modelconvert/config.hpp"
#include "tools/modelconvert/displaylist.hpp"
#include "tools/modelconvert/gbi.hpp"
#include "tools/modelconvert/mesh.hpp"

#include <algorithm>
#include <cassert>
#include <limits>
#include <stdexcept>

#include <fmt/core.h>

namespace modelconvert {
namespace gbi {

namespace {

// =============================================================================

struct VOrder {
    int index;
    std::array<int16_t, 3> pos;
    std::array<int8_t, 3> normal;
    bool same;
};

// Order by position: Z descending, X ascending, Y ascending, then by normal.
bool operator<(const VOrder &x, const VOrder &y) {
    if (x.pos != y.pos) {
        return x.pos < y.pos;
    }
    if (x.normal != y.normal) {
        return x.normal < y.normal;
    }
    return x.index < y.index;
}

// =============================================================================

// Vertex state.
struct VState {
    Vtx vertex;
    int tri_count;
    int group_id;
};

// Group state.
struct GState {
    int tri_count;
    bool can_reuse;
    bool in_current_batch;
    int current_attr; // Vertex ID where attributes are from, or -1.
};

// =============================================================================

void Sort3(std::array<int, 3> &arr) {
    using std::swap;
    if (arr[0] > arr[1])
        swap(arr[0], arr[1]);
    if (arr[1] > arr[2])
        swap(arr[1], arr[2]);
    if (arr[0] > arr[1])
        swap(arr[0], arr[1]);
}

class Compiler {
public:
    Compiler(const Mesh &mesh, const Config &cfg, std::FILE *stats) {
        if (mesh.vertex.size() > std::numeric_limits<int>::max()) {
            throw std::runtime_error("too many vertexes");
        }
        int nvert = mesh.vertex.size();
        if (nvert == 0) {
            return;
        }

        m_vertex.resize(nvert);
        const std::vector<std::array<int16_t, 3>> *vertexpos =
            &mesh.animation_frame.at(0);
        if (cfg.animate) {
            vertexpos = &mesh.animation_frame.at(4);
        }
        for (int i = 0; i < nvert; i++) {
            VState &v = m_vertex.at(i);
            v.vertex.pos = vertexpos->at(i);
            const VertexAttr &vv = mesh.vertex.at(i);
            v.vertex.pad = 0;
            v.vertex.texcoord = vv.texcoord;
            if (cfg.use_vertex_colors) {
                v.vertex.color = vv.color;
            } else if (cfg.use_normals) {
                for (int i = 0; i < 3; i++) {
                    v.vertex.color[i] = vv.normal[i];
                }
                v.vertex.color[3] = 0;
            } else {
                v.vertex.color = std::array<uint8_t, 4>{{0, 0, 0, 0}};
            }
            v.tri_count = 0;
            v.group_id = -1;
        }
        m_triangle = mesh.triangle;
        for (const Triangle &tri : m_triangle) {
            for (const int idx : tri.vertex) {
                m_vertex.at(idx).tri_count++;
            }
        }

        // Sort vertexes.
        std::vector<VOrder> vorder(nvert);
        for (int i = 0; i < nvert; i++) {
            VOrder &v = vorder.at(i);
            v.index = i;
            v.pos = vertexpos->at(i);
            const VertexAttr &d = mesh.vertex.at(i);
            v.normal = d.normal;
            v.same = false;
        }
        std::sort(std::begin(vorder), std::end(vorder));

        // Figure out which vertexes are different.
        vorder.at(0).same = false;
        for (int i = 1; i < nvert; i++) {
            VOrder &x = vorder.at(i), &y = vorder.at(i - 1);
            x.same = x.pos == y.pos && x.normal == y.normal;
        }

        // Assign each vertex to a group.
        {
            int group_id = -1;
            for (int i = 0; i < nvert; i++) {
                VOrder &v = vorder.at(i);
                if (!v.same) {
                    group_id = m_group.size();
                    GState group{};
                    group.current_attr = -1;
                    m_group.push_back(group);
                }
                VState &vv = m_vertex.at(v.index);
                vv.group_id = group_id;
                GState &g = m_group.back();
                g.tri_count += vv.tri_count;
            }
        }

        if (stats) {
            fmt::print(stats, "    Raw vertex count: {}\n", nvert);
            fmt::print(stats, "    Unique vertex positions: {}\n",
                       m_group.size());
        }
    }

    void Emit(DisplayList *dl, std::FILE *stats) {
        // For each batch.
        while (!m_triangle.empty()) {
            StartBatch(dl);
            while (true) {
                int next_tri = BestTriangle();
                if (next_tri == -1) {
                    break;
                }
                AddTriangle(next_tri);
            }
            EmitPrevBatch(dl, stats);
            std::swap(m_batch_vertex, m_prev_vertex);
            std::swap(m_batch_triangle, m_prev_triangle);
        }
        EmitPrevBatch(dl, stats);
        if (stats) {
            fmt::print(stats, "    Final vertex count: {} ({:.2f}x)\n",
                       m_total_vtx,
                       static_cast<double>(m_total_vtx) /
                           static_cast<double>(m_group.size()));
        }
    }

private:
    int BestTriangle() const {
        int best = -1;
        using Cost = std::array<int, 5>;
        Cost best_cost{};
        int n = m_triangle.size();
        for (int i = 0; i < n; i++) {
            const std::array<int, 3> tri = m_triangle.at(i).vertex;
            int space_required = 0;
            int transforms = 0;
            std::array<int, 3> num_tris;
            for (int j = 0; j < 3; j++) {
                const int vertex_id = tri[j];
                const VState &v = m_vertex.at(vertex_id);
                const GState &g = m_group.at(v.group_id);
                num_tris[j] = g.tri_count;
                if (!g.in_current_batch) {
                    space_required++;
                    if (!g.can_reuse) {
                        transforms++;
                    }
                }
            }
            Sort3(num_tris);
            Cost cost{space_required, transforms, num_tris[0], num_tris[1],
                      num_tris[2]};
            if (space_required <= m_vert_space &&
                (cost < best_cost || best == -1)) {
                best = i;
                best_cost = cost;
            }
        }
        return best;
    }

    void AddTriangle(int triangle_id) {
        Triangle tri = m_triangle.at(triangle_id);
        m_triangle.erase(m_triangle.begin() + triangle_id);
        for (const int vertex_id : tri.vertex) {
            VState &v = m_vertex.at(vertex_id);
            GState &g = m_group.at(v.group_id);
            if (!g.in_current_batch) {
                assert(m_vert_space > 0);
                m_vert_space--;
                m_batch_vertex.push_back(vertex_id);
            }
            assert(v.tri_count >= 1);
            v.tri_count--;
            assert(g.tri_count >= 1);
            g.tri_count--;
            g.in_current_batch = true;
            g.current_attr = v.tri_count == 0 ? -1 : vertex_id;
        }
        m_batch_triangle.push_back(tri);
    }

    void StartBatch(DisplayList *dl) {
        for (GState &g : m_group) {
            g.can_reuse = false;
            g.in_current_batch = false;
        }
        for (const int vertex_id : m_prev_vertex) {
            const VState &v = m_vertex.at(vertex_id);
            GState &g = m_group.at(v.group_id);
            g.can_reuse = true;
        }
        m_vert_space = dl->vertex_cache_size();
        m_batch_vertex.clear();
        m_batch_triangle.clear();
    }

    void EmitPrevBatch(DisplayList *dl, std::FILE *stats) {
        (void)dl;
        const std::vector<int> &vertex = m_prev_vertex;
        const std::vector<Triangle> &triangle = m_prev_triangle;
        if (vertex.empty() && triangle.empty()) {
            return;
        }
        m_total_vtx += vertex.size();
        int batch_index = m_batch_index++;
        bool high_index = (batch_index & 1) != 0;

        // Figure out which slots have vertexes in this batch.
        int cache_size = dl->vertex_cache_size();
        std::vector<uint8_t> reuse_slot(cache_size, 0);
        for (const int vertex_id : vertex) {
            const VState &v = m_vertex.at(vertex_id);
            int slot = dl->cache().CachePos(v.vertex.pos);
            if (slot >= 0) {
                reuse_slot[slot] = 1;
            }
        }

        // Figure out how which slots we will reuse.
        int count = vertex.size();
        if (count > cache_size) {
            throw std::runtime_error(
                "Batch::EmitVertexes failed: too many vertexes");
        }
        int start;
        if (high_index) {
            int pos = 0;
            while (pos + count < cache_size) {
                if (reuse_slot.at(pos)) {
                    count--;
                }
                pos++;
            }
            start = pos;
        } else {
            int pos = cache_size;
            while (pos > count) {
                if (reuse_slot.at(pos - 1)) {
                    count--;
                }
                pos--;
            }
            start = 0;
        }
        int end = start + count;
        for (int i = start; i < end; i++) {
            reuse_slot.at(i) = 0;
        }

        // Figure out which vertexes this batch will transform.
        std::vector<int> transform_verts;
        transform_verts.reserve(count);
        for (const int vertex_id : vertex) {
            const VState &v = m_vertex.at(vertex_id);
            int slot = dl->cache().CachePos(v.vertex.pos);
            if (slot < 0 || !reuse_slot.at(slot)) {
                transform_verts.push_back(vertex_id);
            }
        }

        // Transform vertexes.
        {
            std::vector<Vtx> vdata(transform_verts.size());
            int vstart = 0, vend = transform_verts.size();
            for (const int vertex_id : transform_verts) {
                const VState &v = m_vertex.at(vertex_id);
                const GState &g = m_group.at(v.group_id);
                int index;
                if (g.in_current_batch == high_index) {
                    index = --vend;
                } else {
                    index = vstart++;
                }
                vdata.at(index) = v.vertex;
            }
            assert(vstart == vend);
            dl->Vertex(start, vdata);
        }

        // Emit triangles.
        for (const Triangle &tri : triangle) {
            std::array<int, 3> tidx;
            for (int i = 0; i < 3; i++) {
                const int vertex_id = tri.vertex[i];
                const VState &v = m_vertex.at(vertex_id);
                int slot = dl->cache().CachePos(v.vertex.pos);
                if (slot < 0) {
                    throw std::runtime_error(
                        "Batch::EmitVertexes: vertex missing from cache");
                }
                tidx[i] = slot;
                const Vtx *sv = dl->cache().Get(slot);
                if (sv == nullptr) {
                    throw std::runtime_error("missing slot data");
                }
                dl->SetVertexTexcoord(slot, v.vertex.texcoord);
            }
            dl->Triangle(tidx);
        }

        if (stats) {
            fmt::print(stats, "    Batch {}: vertexes={}, triangles={}\n",
                       batch_index, vertex.size(), triangle.size());
        }
    }

    // The mesh data to emit.
    std::vector<VState> m_vertex;
    std::vector<GState> m_group;
    std::vector<Triangle> m_triangle;

    // Space remaining in vetrex cache in current batch.
    int m_vert_space;

    // Vertexes to be transformed in current batch, previous batch.
    std::vector<int> m_batch_vertex;
    std::vector<int> m_prev_vertex;

    // Triangles in current batch, previous batch.
    std::vector<Triangle> m_batch_triangle;
    std::vector<Triangle> m_prev_triangle;

    // Which batch this is.
    int m_batch_index = 0;

    // Total vertex count at end, including duplicates that were not merged.
    int m_total_vtx = 0;
};

} // namespace

void CompileMesh(DisplayList *dl, const Mesh &mesh, const Config &cfg,
                 std::FILE *stats) {
    if (stats) {
        fmt::print(stats, "Compiling model\n");
    }
    Compiler compiler{mesh, cfg, stats};
    compiler.Emit(dl, stats);
    dl->End();
}

} // namespace gbi
} // namespace modelconvert
