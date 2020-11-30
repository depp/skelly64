#include "tools/modelconvert/mesh.hpp"

#include "tools/util/quote.hpp"

#include <algorithm>
#include <assimp/material.h>
#include <assimp/mesh.h>
#include <assimp/scene.h>
#include <cassert>
#include <fmt/core.h>

namespace modelconvert {

std::string_view Str(const aiString &s) {
    return std::string_view(s.data, s.length);
}

namespace {

[[noreturn]] void BadVertexPos(unsigned idx, const std::array<float, 3> &vec,
                               const char *msg) {
    throw MeshError(
        fmt::format("error processing vertex {} at ({}, {}, {}): {}", idx,
                    vec[0], vec[1], vec[2], msg));
}

std::array<float, 3> ImportVector(const aiVector3D &v) {
    return {{v.x, v.y, v.z}};
}

std::array<uint8_t, 4> ImportColor(const aiColor4D &color) {
    std::array<float, 4> arr = {{color.r, color.g, color.b, color.a}};
    std::array<uint8_t, 4> rgba;
    for (int i = 0; i < 4; i++) {
        float c = arr[i];
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

void MeshAddNodes(Mesh &mesh, aiNode *node, int parent) {
    size_t n = mesh.nodes.size();
    if (n > std::numeric_limits<int>::max()) {
        throw std::runtime_error("too many nodes");
    }
    int index = n;
    mesh.nodes.emplace_back(parent, std::string(Str(node->mName)));
    for (aiNode **cp = node->mChildren, **ce = cp + node->mNumChildren;
         cp != ce; cp++) {
        MeshAddNodes(mesh, *cp, index);
    }
}

// Find the index of the bone with the given name, or throw an exception.
uint32_t FindBone(Mesh &mesh, std::string_view name) {
    std::string bone_name(name);
    auto bonep = mesh.bone_names.find(bone_name);
    if (bonep != mesh.bone_names.end()) {
        return bonep->second;
    }
    auto start = std::begin(mesh.nodes), end = std::end(mesh.nodes);
    auto ptr = start;
    while (ptr != end && ptr->name != bone_name) {
        ++ptr;
    }
    if (ptr == end) {
        throw std::runtime_error(fmt::format("could not find bone, name={}",
                                             util::Quote(bone_name)));
    }
    uint32_t bone_id = ptr - start;
    for (++ptr; ptr != end; ++ptr) {
        if (ptr->name == bone_name) {
            throw std::runtime_error(
                fmt::format("bone matches name of multiple nodes, name={}",
                            util::Quote(bone_name)));
        }
    }
    mesh.bone_names.emplace(std::move(bone_name), bone_id);
    return bone_id;
}

} // namespace

void Mesh::AddNodes(const Config &cfg, std::FILE *stats, aiNode *node) {
    (void)stats;
    if (cfg.animate) {
        MeshAddNodes(*this, node, -1);
    }
}

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
            std::array<float, 3> fpos = ImportVector(posarr[i]);
            fpos = cfg.axes.Apply(fpos);
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
        if (normarr == nullptr) {
            if (stats != nullptr) {
                fmt::print(stats, "No normals\n");
            }
            goto done_normals;
        }
        for (unsigned i = 0; i < nvert; i++) {
            std::array<float, 3> fnorm = ImportVector(normarr[i]);
            fnorm = cfg.axes.Apply(fnorm);
            std::array<int8_t, 3> inorm;
            for (int j = 0; j < 3; j++) {
                // 11.7.2 Normal Vector Normalization: limit to 127, but Q&A 3D
                // Calculations Q3 explains that 128 is 1.0.
                const float v = fnorm[j] * 128.0f;
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
done_normals:

    // Get texture coordinates.
    if (cfg.use_texcoords) {
        const aiVector3D *texcoordarr = mesh->mTextureCoords[0];
        if (texcoordarr == nullptr) {
            if (stats != nullptr) {
                fmt::print(stats, "No texture coordinates\n");
            }
            goto done_texcoords;
        }
        if (cfg.texcoord_bits < 0 || cfg.texcoord_bits >= 32) {
            throw std::range_error("texcoord_bits out of range");
        }
        const float scale = 1 << cfg.texcoord_bits;
        for (unsigned i = 0; i < nvert; i++) {
            std::array<float, 3> ftexcoord = ImportVector(texcoordarr[i]);
            ftexcoord[1] = 1.0f - ftexcoord[1];
            std::array<int16_t, 2> itexcoord;
            for (int j = 0; j < 2; j++) {
                const float v = ftexcoord[j] * scale;
                int iv;
                if (v > std::numeric_limits<int16_t>::min()) {
                    if (v < std::numeric_limits<int16_t>::max()) {
                        iv = std::lrintf(v);
                    } else {
                        iv = std::numeric_limits<int16_t>::max();
                    }
                } else {
                    iv = std::numeric_limits<int16_t>::min();
                }
                itexcoord[j] = iv;
            }
            mverts[i].texcoord = itexcoord;
        }
    }
done_texcoords:

    // Get vertex colors.
    if (cfg.use_vertex_colors) {
        const aiColor4D *colorarr = mesh->mColors[0];
        if (colorarr == nullptr) {
            if (stats != nullptr) {
                fmt::print(stats, "No colors\n");
            }
            goto done_colors;
        }
        for (unsigned i = 0; i < nvert; i++) {
            std::array<uint8_t, 4> icolor = ImportColor(colorarr[i]);
            mverts[i].color =
                std::array<uint8_t, 3>{{icolor[0], icolor[1], icolor[2]}};
            mverts[i].alpha = icolor[3];
        }
    }
done_colors:

    // Add bone weights.
    if (cfg.animate && mesh->mNumBones > 0) {
        for (aiBone **bp = mesh->mBones, **be = bp + mesh->mNumBones; bp != be;
             bp++) {
            aiBone *bone = *bp;
            assert(bone != nullptr);
            uint32_t bone_id = FindBone(*this, Str(bone->mName));
            for (const aiVertexWeight *wp = bone->mWeights,
                                      *we = wp + bone->mNumWeights;
                 wp != we; wp++) {
                const float fweight = wp->mWeight;
                const int iweight =
                    std::lrintf(static_cast<float>(MaxBoneWeight) * fweight);
                if (iweight != 0) {
                    unsigned index = wp->mVertexId;
                    Vertex &v = mverts.at(index);
                    size_t num_bones = 0;
                    while (num_bones < MaxBones &&
                           v.bone_weights[num_bones].weight > 0) {
                        num_bones++;
                    }
                    if (num_bones == MaxBones) {
                        throw std::runtime_error(fmt::format(
                            "vertex has too many bones, vertex={}", index));
                    }
                    size_t pos;
                    for (pos = 0; pos < num_bones; pos++) {
                        const BoneWeight &w = v.bone_weights[pos];
                        if (w.bone > bone_id) {
                            break;
                        }
                    }
                    if (pos < num_bones) {
                        std::copy_backward(
                            std::begin(v.bone_weights) + pos,
                            std::begin(v.bone_weights) + num_bones,
                            std::begin(v.bone_weights) + num_bones + 1);
                    }
                    BoneWeight &w = v.bone_weights[pos];
                    w.bone = bone_id;
                    w.weight = iweight;
                }
            }
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