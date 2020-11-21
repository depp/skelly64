#include "tools/modelconvert/mesh.hpp"

#include "tools/util/quote.hpp"

#include <assimp/material.h>
#include <assimp/mesh.h>
#include <fmt/core.h>

namespace modelconvert {

std::string_view Str(const aiString &s) {
    return std::string_view(s.data, s.length);
}

namespace {

[[noreturn]] void BadVertexPos(unsigned idx, const aiVector3D &vec,
                               const char *msg) {
    throw MeshError(
        fmt::format("error processing vertex {} at ({}, {}, {}): {}", idx,
                    vec[0], vec[1], vec[2], msg));
}

} // namespace

void Mesh::AddMesh(const Config &cfg, std::FILE *stats, aiMesh *mesh) {
    unsigned nvert = mesh->mNumVertices;
    std::vector<Vertex> mverts(nvert, Vertex{});
    using Vector = std::array<int16_t, 3>;
    using Traits = VectorTraits<int16_t, 3>;

    // Get vertex positions.
    {
        const float scale = cfg.scale;
        const aiVector3D *posarr = mesh->mVertices;
        for (unsigned i = 0; i < nvert; i++) {
            const aiVector3D fpos = posarr[i];
            std::array<int16_t, 3> ipos;
            for (int j = 0; j < 3; j++) {
                const float v = fpos[j] * scale;
                if (v > std::numeric_limits<int16_t>::min()) {
                    if (v < std::numeric_limits<int16_t>::max()) {
                        ipos[j] = std::lrintf(v);
                    } else {
                        BadVertexPos(i, fpos, "coordinate out of range");
                    }
                } else {
                    BadVertexPos(i, fpos, "coordinate out of range");
                }
            }
            mverts[i].pos = ipos;
        }
    }

    // Compute bounds.
    {
        Vector min = Traits::max_value(), max = Traits::min_value();
        for (const Vertex &vec : mverts) {
            min = Traits::min(min, vec.pos);
            max = Traits::max(max, vec.pos);
        }
        if (stats != nullptr) {
            fmt::print(stats, "Submesh bounds: ({}, {}, {}) ({}, {}, {})\n",
                       min[0], min[1], min[2], max[0], max[1], max[2]);
        }
        if (vertexes.vertexes.empty()) {
            bounds_min = min;
            bounds_max = max;
        } else {
            bounds_min = Traits::min(bounds_min, min);
            bounds_max = Traits::max(bounds_max, max);
        }
    }

    // Get vertex normals.
    if (cfg.use_normals) {
        const aiVector3D *normarr = mesh->mNormals;
        for (unsigned i = 0; i < nvert; i++) {
            const aiVector3D fnorm = normarr[i];
            std::array<int8_t, 3> inorm;
            for (int j = 0; j < 3; j++) {
                // 11.7.2 Normal Vector Normalization: scale to 127.
                const float v = fnorm[j] * 127.0f;
                int iv;
                if (v > std::numeric_limits<int8_t>::min()) {
                    if (v < std::numeric_limits<int8_t>::max()) {
                        iv = std::lrintf(v);
                    } else {
                        iv = std::numeric_limits<int8_t>::max();
                    }
                } else {
                    iv = std::numeric_limits<int8_t>::min();
                }
                inorm[j] = iv;
            }
            mverts[i].normal = inorm;
        }
    }

    // Add vertexes to vertex set, get indexes.
    std::vector<unsigned> mvindex(nvert, 0);
    for (unsigned i = 0; i < nvert; i++) {
        unsigned idx = vertexes.Add(mverts[i]);
        mvindex[i] = idx;
    }

    // Add triangles.
    {
        unsigned material = mesh->mMaterialIndex;
        unsigned nfaces = mesh->mNumFaces;
        const aiFace *faces = mesh->mFaces;
        for (unsigned i = 0; i < nfaces; i++) {
            unsigned nindex = faces[i].mNumIndices;
            const unsigned *indexes = faces[i].mIndices;
            if (nindex != 3) {
                throw MeshError(
                    fmt::format("face has {} vertexes, should have 3", nindex));
            }
            Triangle tri{};
            tri.material = material;
            for (unsigned j = 0; j < nindex; j++) {
                unsigned idx = indexes[j];
                if (idx >= mvindex.size()) {
                    throw MeshError("invalid vertex index");
                }
                tri.vertex[j] = mvindex[idx];
            }
            triangles.push_back(tri);
        }
    }
}

namespace {

std::array<uint8_t, 4> ImportColor(const aiColor4D &color) {
    std::array<uint8_t, 4> rgba;
    for (int i = 0; i < 4; i++) {
        float c = color[i];
        int v = 255;
        if (c < 1.0f) {
            if (c > 0.0f) {
                v = std::lrintf(256.0f * c);
                if (v > 255) {
                    v = 255;
                }
            } else {
                v = 0;
            }
        }
        rgba[i] = v;
    }
    return rgba;
}

} // namespace

Material Material::Import(const Config &cfg, aiMaterial *mat) {
    // for (aiMaterialProperty **ptr = mat->mProperties,
    //                         **end = ptr + mat->mNumProperties;
    //      ptr != end; ptr++) {
    //     aiMaterialProperty *prop = *ptr;
    //     fmt::print("Name = {}\n", util::Quote(Str(prop->mKey)));
    // }
    Material m = Default();
    if (cfg.use_primitive_color) {
        aiColor4D diffuse{1.0f, 1.0f, 1.0f, 1.0f};
        if (mat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) != AI_SUCCESS) {
            throw MeshError("could not get diffuse color");
        }
        m.rgba = ImportColor(diffuse);
    }

    return m;
}

} // namespace modelconvert