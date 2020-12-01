#include "tools/modelconvert/mesh.hpp"

#include "tools/modelconvert/config.hpp"

#include <assimp/scene.h>
#include <cassert>
#include <fmt/core.h>

namespace modelconvert {

std::string_view Str(const aiString &s) {
    return std::string_view(s.data, s.length);
}

namespace {

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

// Quantize a floating-point 3D vector to a 16-bit integer 3D vector.
std::array<int16_t, 3> QuantizeVector(aiVector3D v) {
    std::array<int16_t, 3> r;
    for (int i = 0; i < 3; i++) {
        float fx = v[i];
        int ix;
        if (std::isnan(fx)) {
            ix = 0;
        } else if (fx < std::numeric_limits<int16_t>::min()) {
            ix = std::numeric_limits<int16_t>::min();
        } else if (fx > std::numeric_limits<int16_t>::max()) {
            ix = std::numeric_limits<int16_t>::max();
        } else {
            ix = std::lrintf(fx);
        }
        r[i] = ix;
    }
    return r;
}

} // namespace

Mesh Mesh::Import(const Config &cfg, std::FILE *stats, const aiScene *scene) {
    Mesh mesh;
    aiMatrix4x4 scale;
    aiMatrix4x4::Scaling(aiVector3D(cfg.scale), scale);
    mesh.m_transform = cfg.axes.ToMatrix() * scale;
    mesh.AddNodes(scene->mRootNode, -1);
    mesh.AddMeshes(cfg, stats, scene, scene->mRootNode, aiMatrix4x4());
    mesh.ComputeStaticPos();
    return mesh;
}

void Mesh::Dump(std::FILE *stats) const {
    assert(stats != nullptr);
    fmt::print(stats,
               "Imported model:\n"
               "    Vertexes: {}\n    Triangles: {}\n"
               "    Nodes: {}\n    Bones: {}\n",
               m_vertex.size(), m_triangle.size(), m_node.size(),
               m_bone_names.size());
}

void Mesh::AddNodes(const aiNode *node, int parent) {
    size_t n = m_node.size();
    if (n > std::numeric_limits<int>::max()) {
        throw std::runtime_error("too many nodes");
    }
    int index = n;
    m_node.emplace_back(parent, std::string(Str(node->mName)));
    for (aiNode **cp = node->mChildren, **ce = cp + node->mNumChildren;
         cp != ce; cp++) {
        AddNodes(*cp, index);
    }
}

void Mesh::AddMeshes(const Config &cfg, std::FILE *stats, const aiScene *scene,
                     const aiNode *node, const aiMatrix4x4 &transform) {
    const aiMatrix4x4 node_transform = transform * node->mTransformation;
    for (const unsigned *mp = node->mMeshes, *me = mp + node->mNumMeshes;
         mp != me; mp++) {
        unsigned mesh_id = *mp;
        if (mesh_id >= scene->mNumMeshes) {
            throw MeshError("bad mesh reference in scene");
        }
        AddMesh(cfg, stats, scene->mMeshes[mesh_id], node_transform);
    }
    for (aiNode **cp = node->mChildren, **ce = cp + node->mNumChildren;
         cp != ce; cp++) {
        AddMeshes(cfg, stats, scene, *cp, node_transform);
    }
}

void Mesh::AddMesh(const Config &cfg, std::FILE *stats, const aiMesh *mesh,
                   const aiMatrix4x4 &transform) {
    if (mesh->mNumVertices > std::numeric_limits<int>::max()) {
        throw std::runtime_error("too many vertexes");
    }
    const int nvert = mesh->mNumVertices;
    if (m_vertex.size() >
        static_cast<size_t>(std::numeric_limits<int>::max() - nvert)) {
        throw MeshError("too many vertexes");
    }
    const int offset = m_vertex.size();
    m_rawposition.resize(offset + nvert);
    m_vertex.resize(offset + nvert, VertexAttr{});

    // Get vertex positions.
    {
        const aiVector3D *posarr = mesh->mVertices;
        for (int i = 0; i < nvert; i++) {
            m_rawposition.at(offset + i) = transform * posarr[i];
        }
    }

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
        for (int i = 0; i < nvert; i++) {
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
            m_vertex.at(offset + i).texcoord = itexcoord;
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
        for (int i = 0; i < nvert; i++) {
            m_vertex.at(offset + i).color = ImportColor(colorarr[i]);
        }
    }
done_colors:

    // Get vertex normals.
    if (cfg.use_normals) {
        const aiVector3D *normarr = mesh->mNormals;
        if (normarr == nullptr) {
            if (stats != nullptr) {
                fmt::print(stats, "No normals\n");
            }
            goto done_normals;
        }
        for (int i = 0; i < nvert; i++) {
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
            m_vertex.at(offset + i).normal = inorm;
        }
    }
done_normals:;

    {
        int material = mesh->mMaterialIndex;
        int nfaces = mesh->mNumFaces;
        const aiFace *faces = mesh->mFaces;
        for (int i = 0; i < nfaces; i++) {
            int nindex = faces[i].mNumIndices;
            const unsigned *indexes = faces[i].mIndices;
            if (nindex != 3) {
                throw MeshError(
                    fmt::format("face is not a triangle, vertexes={}", nindex));
            }
            Triangle tri{};
            tri.material = material;
            for (int j = 0; j < nindex; j++) {
                unsigned idx = indexes[j];
                if (idx >= static_cast<unsigned>(nvert)) {
                    throw MeshError("invalid vertex index");
                }
                tri.vertex[j] = offset + idx;
            }
            m_triangle.push_back(tri);
        }
    }
}

void Mesh::ComputeStaticPos() {
    m_vertexpos.resize(m_vertex.size());
    for (size_t i = 0; i < m_vertex.size(); i++) {
        m_vertexpos.at(i) = QuantizeVector(m_transform * m_rawposition.at(i));
    }
}

} // namespace modelconvert
