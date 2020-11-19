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

void Mesh::AddMesh(aiMesh *mesh, float scale) {
    unsigned nvert = mesh->mNumVertices;
    std::vector<Vertex> mverts(nvert, Vertex{});

    // Get vertex positions.
    {
        const aiVector3D *posarr = mesh->mVertices;
        for (unsigned i = 0; i < nvert; i++) {
            aiVector3D fpos = posarr[i];
            std::array<int16_t, 3> ipos;
            for (int j = 0; j < 3; j++) {
                float v = fpos[j] * scale;
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

Material Material::Import(aiMaterial *mat) {
    // for (aiMaterialProperty **ptr = mat->mProperties,
    //                         **end = ptr + mat->mNumProperties;
    //      ptr != end; ptr++) {
    //     aiMaterialProperty *prop = *ptr;
    //     fmt::print("Name = {}\n", util::Quote(Str(prop->mKey)));
    // }
    aiColor4D diffuse{1.0f, 1.0f, 1.0f, 1.0f};
    if (mat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) != AI_SUCCESS) {
        throw MeshError("could not get diffuse color");
    }
    Material m = Default();
    for (int i = 0; i < 4; i++) {
        float c = diffuse[i];
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
        m.rgba[i] = v;
    }
    return m;
}

} // namespace modelconvert