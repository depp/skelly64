#include "tools/modelconvert/mesh.hpp"

#include "tools/modelconvert/gbi.hpp"
#include "tools/util/bswap.hpp"

#include <algorithm>
#include <limits>

#include <fmt/core.h>

namespace modelconvert {

void Material::Write(const Material &state, std::vector<Gfx> *dl) const {
    if (rgba != state.rgba) {
        dl->push_back(Gfx::DPSetPrimColor(0, 0, rgba));
    }
}

Material Material::Default() {
    return Material{{{255, 255, 255, 255}}};
}

namespace {

struct Header {
    void Write(uint8_t *ptr) const { (void)ptr; }
};

} // namespace

std::vector<uint8_t> BatchMesh::EmitGBI(
    const Config &cfg, const std::vector<Material> &materials) const {
    const size_t headerlen = 0;
    const size_t magiclen = 16;

    // Emit the display list.
    std::vector<std::pair<unsigned, const Batch *>> batch_offsets;
    std::vector<Gfx> dl;
    std::vector<Triangle> tris;
    unsigned cur_mat = std::numeric_limits<unsigned>::max();
    Material mat = Material::Default();
    for (const Batch &batch : batches) {
        // Load the vertexes. We don't know what the correct address is yet, so
        // we just put a hole here.
        batch_offsets.emplace_back(dl.size(), &batch);
        dl.push_back(Gfx{});

        // Put the triangles in the correct order.
        tris = batch.triangles;
        auto start = tris.begin();
        auto end = tris.end();
        // Put triangles with the current material at the beginning.
        auto ptr =
            std::partition(start, end, [cur_mat](const Triangle &tri) -> bool {
                return tri.material == cur_mat;
            });
        // Sort the remaining triangles by material.
        std::sort(ptr, end, [](const Triangle &x, const Triangle &y) -> bool {
            return x.material < y.material;
        });

        // Emit the triangles.
        const Triangle *pending = nullptr;
        for (const Triangle &tri : tris) {
            if (cur_mat != tri.material) {
                if (pending != nullptr) {
                    dl.push_back(Gfx::SP1Triangle(pending->vertex));
                    pending = nullptr;
                }
                const Material &new_mat = materials.at(tri.material);
                new_mat.Write(mat, &dl);
                cur_mat = tri.material;
                mat = new_mat;
            }
            if (pending != nullptr) {
                dl.push_back(Gfx::SP2Triangle(pending->vertex, tri.vertex));
                pending = nullptr;
            } else {
                pending = &tri;
            }
        }
        if (pending != nullptr) {
            dl.push_back(Gfx::SP1Triangle(pending->vertex));
        }
    }
    dl.push_back(Gfx::SPEndDisplayList());
    // Fix up the display list, now that we know the length.
    size_t dllen = headerlen + dl.size() * GfxSize;
    for (const auto &bo : batch_offsets) {
        const Batch &batch = *bo.second;
        dl[bo.first] =
            Gfx::SPVertex(RSPAddress(dllen + VtxSize * batch.vert_src),
                          batch.vert_count, batch.vert_dest);
    }

    // Emit the data.
    size_t vtxlen = vertexes.size() * VtxSize;
    std::vector<uint8_t> data;
    data.resize(magiclen + headerlen + dllen + vtxlen);
    const std::string_view magic{"Model"};
    std::copy(magic.begin(), magic.end(), data.begin());
    Header header{};
    header.Write(data.data() + magiclen);

    uint8_t *ptr = data.data() + magiclen + headerlen;
    for (const Gfx &g : dl) {
        g.Write(ptr);
        ptr += GfxSize;
    }
    if (cfg.use_normals) {
        for (const Vertex &v : vertexes) {
            VtxC vc{};
            vc.pos = v.pos;
            vc.texcoord = v.texcoord;
            for (int i = 0; i < 3; i++) {
                vc.color[i] = v.normal[i];
            }
            vc.color[3] = v.alpha;
            vc.Write(ptr);
            ptr += VtxSize;
        }
    } else {
        for (const Vertex &v : vertexes) {
            VtxC vc{};
            vc.pos = v.pos;
            vc.texcoord = v.texcoord;
            for (int i = 0; i < 3; i++) {
                vc.color[i] = v.color[i];
            }
            vc.color[3] = v.alpha;
            vc.Write(ptr);
            ptr += VtxSize;
        }
    }
    return data;
}

} // namespace modelconvert
