#include "tools/modelconvert/mesh.hpp"

#include <algorithm>
#include <cassert>
#include <limits>

#include <fmt/core.h>

namespace modelconvert {

namespace {

constexpr unsigned UMax = std::numeric_limits<unsigned>::max();

// State for each vertex.
struct VState {
    bool in_cache;  // Currently in cache.
    bool can_reuse; // Can reuse from previous batch.
    unsigned tris;  // Number of remaining triangles using this vertex.
};

void Sort3(unsigned *arr) {
    using std::swap;
    if (arr[0] > arr[1])
        swap(arr[0], arr[1]);
    if (arr[1] > arr[2])
        swap(arr[1], arr[2]);
    if (arr[0] > arr[1])
        swap(arr[0], arr[1]);
}

// Cost for adding a triangle.
struct Cost {
    unsigned cache_used;   // Additional cache entries needed.
    unsigned new_verts;    // Number of new vertexes to transform.
    unsigned vert_tris[3]; // Number of triangles on each vertex, ascending.

    bool operator<(const Cost &c) const {
        if (cache_used != c.cache_used)
            return cache_used < c.cache_used;
        if (new_verts != c.new_verts)
            return new_verts < c.new_verts;
        for (int i = 0; i < 3; i++) {
            if (vert_tris[i] != c.vert_tris[i])
                return vert_tris[i] < c.vert_tris[i];
        }
        return false;
    }

    // Evaluate the cost of adding the given triangle to the display list, given
    // the current state.
    static Cost Evaluate(const std::vector<VState> &vstate,
                         const Triangle &tri) {
        Cost cost;
        cost.cache_used = 0;
        cost.new_verts = 0;
        for (int i = 0; i < 3; i++) {
            unsigned idx = tri.vertex[i];
            const VState &st = vstate.at(idx);
            if (!st.in_cache) {
                cost.cache_used++;
                if (!st.can_reuse) {
                    cost.new_verts++;
                }
            }
            cost.vert_tris[i] = st.tris;
        }
        Sort3(cost.vert_tris);
        return cost;
    }
};

// State of the vertex cache.
class CacheState {
public:
    // Returns index of the given vertex, or UMax.
    unsigned CachePos(unsigned vtx) {
        if (vtx >= v2c.size()) {
            return UMax;
        }
        return v2c.at(vtx);
    }

    // Place the vertex at the given index in the cache.
    void Set(unsigned idx, unsigned vtx) {
        if (idx >= c2v.size()) {
            c2v.resize(idx + 1, UMax);
        }
        if (vtx >= v2c.size()) {
            v2c.resize(vtx + 1, UMax);
        }
        c2v.at(idx) = vtx;
        v2c.at(vtx) = idx;
    }

private:
    std::vector<unsigned> v2c;
    std::vector<unsigned> c2v;
};

struct BuildBatch {
    // Triangles rendered in this batch.
    std::vector<Triangle> triangles;
    // Vertexes used by triangles in this batch.
    std::vector<unsigned> vertexes;
    // Whether the vertexes are being added "upwards" or "downwards". If false,
    // vertexes are added upwards, and vertexes reused from the previous batch
    // are at low indexes. If true, vertexes are added downwards, and vertexes
    // reused from the previous batch are at high indexes.
    bool downwards;

    bool empty() const { return triangles.empty(); }

    void Clear(bool downwards) {
        triangles.clear();
        vertexes.clear();
        this->downwards = downwards;
    }

    // Emit the batch and add it to the mesh.
    void Emit(BatchMesh *bmesh, CacheState *cache,
              const std::vector<FVertex> &vertdata,
              const std::vector<VState> &vstate, unsigned cache_size) {
        auto start = vertexes.begin();
        auto end = vertexes.end();
        // Put the in_cache items at the beginning if upwards, or in the end if
        // downwards.
        bool value = !downwards;
        std::partition(start, end, [value, &vstate](unsigned vtx) -> bool {
            return vstate.at(vtx).in_cache == value;
        });
        Batch batch;
        // Emit the vertexes and update the cache state.
        unsigned idx = downwards ? cache_size - vertexes.size() : 0;
        batch.vert_count = vertexes.size();
        batch.vert_src = bmesh->vertexes.size();
        batch.vert_dest = idx;
        for (auto it = start; it != end; ++it, ++idx) {
            unsigned vtx = *it;
            bmesh->vertexes.push_back(vertdata.at(vtx).vert);
            cache->Set(idx, vtx);
        }
        // Emit the triangles.
        batch.triangles.reserve(triangles.size());
        for (Triangle tri : triangles) {
            for (int i = 0; i < 3; i++) {
                unsigned vtx = tri.vertex[i];
                unsigned idx = cache->CachePos(vtx);
                assert(idx < cache_size);
                tri.vertex[i] = idx;
            }
            batch.triangles.push_back(tri);
        }
        bmesh->batches.emplace_back(std::move(batch));
    }
};

void swap(BuildBatch &x, BuildBatch &y) {
    using std::swap;
    swap(x.triangles, y.triangles);
    swap(x.vertexes, y.vertexes);
    swap(x.downwards, y.downwards);
}

} // namespace

BatchMesh Mesh::MakeBatches(unsigned cache_size) {
    if (triangles.empty()) {
        return BatchMesh{};
    }

    // Simple greedy algorithm. Add the lowest-cost triangle to each batch until
    // the batch is full. Add batches until no more triangles are left. Each
    // batch can reuse vertexes from the previous batch.

    std::vector<VState> vstate(vertexes.vertexes.size(),
                               VState{false, false, 0});
    std::vector<Triangle> tris{triangles};
    for (const Triangle &tri : tris) {
        for (const unsigned idx : tri.vertex) {
            assert(idx < vstate.size());
            vstate.at(idx).tris++;
        }
    }

    // Create batches in a loop until we run out of triangles.
    BuildBatch previous, current;
    BatchMesh bmesh;
    CacheState cache;
    for (bool downwards = false; !tris.empty(); downwards = !downwards) {
        current.Clear(downwards);
        for (VState &st : vstate) {
            st.in_cache = false;
            st.can_reuse = false;
        }
        for (unsigned vtx : previous.vertexes) {
            vstate.at(vtx).can_reuse = true;
        }
        // Amount of cache space remaining in this batch.
        unsigned cache_space = cache_size;

        // Add triangles to the batch until it can't be done.
        for (;;) {
            // Find the triangle with the lowest cost to add to the batch.
            Cost cost{UMax, UMax, {UMax, UMax, UMax}};
            unsigned best_tri = UMax;
            for (size_t i = 0; i < tris.size(); i++) {
                Cost tcost = Cost::Evaluate(vstate, tris.at(i));
                if (tcost < cost) {
                    cost = tcost;
                    best_tri = i;
                }
            }
            // Could not find any triangle to add.
            if (best_tri == UMax || cost.cache_used > cache_space) {
                break;
            }
            // Add this triangle to the batch.
            const Triangle tri = tris.at(best_tri);
            tris.erase(tris.begin() + best_tri);
            current.triangles.push_back(tri);
            for (int i = 0; i < 3; i++) {
                unsigned idx = tri.vertex[i];
                VState &st = vstate.at(idx);
                st.tris--;
                if (!st.in_cache) {
                    // Add this vertex to the cache, or reuse it.
                    cache_space--;
                    st.in_cache = true;
                    if (!st.can_reuse) {
                        current.vertexes.push_back(idx);
                    }
                }
            }
        }

        // Sanity check, we must terminate.
        assert(!current.triangles.empty());

        // Finish creating the previous batch, now that we know which vertexes
        // need to be reused.
        if (!previous.empty()) {
            previous.Emit(&bmesh, &cache, vertexes.vertexes, vstate,
                          cache_size);
        }

        swap(previous, current);
    }

    previous.Emit(&bmesh, &cache, vertexes.vertexes, vstate, cache_size);
    return bmesh;
}

void BatchMesh::Dump(std::FILE *stats) const {
    fmt::print(stats, "Vertexes:\n");
    for (size_t i = 0; i < vertexes.size(); i++) {
        const Vertex &v = vertexes[i];
        fmt::print(stats, "  {:3d}: ({:+6d}, {:+6d}, {:+6d})\n", i, v.pos[0],
                   v.pos[1], v.pos[2]);
    }
    fmt::print(stats, "Batches:\n");
    for (size_t i = 0; i < batches.size(); i++) {
        const Batch &batch = batches[i];
        fmt::print(stats, "  Batch {}\n", i);
        fmt::print(stats, "    count={} src={} dest={}\n", batch.vert_count,
                   batch.vert_src, batch.vert_dest);
        for (const Triangle &tri : batch.triangles) {
            fmt::print(stats, "    tri {} {} {}\n", tri.vertex[0],
                       tri.vertex[1], tri.vertex[2]);
        }
    }
}

} // namespace modelconvert
