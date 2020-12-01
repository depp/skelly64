#include "tools/modelconvert/mesh.hpp"

#include "tools/modelconvert/config.hpp"
#include "tools/util/quote.hpp"

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
    if (mesh.m_rawposition.empty()) {
        throw MeshError("empty mesh");
    }
    mesh.SetVertexPos(mesh.m_rawposition);
    if (cfg.animate) {
        aiAnimation **ap = scene->mAnimations;
        if (scene->mNumAnimations >= 2) {
            mesh.AddAnimation(ap[1]);
        }
    }
    if (stats) {
        float min[3], max[3];
        min[0] = max[0] = mesh.m_rawposition[0].x;
        min[1] = max[1] = mesh.m_rawposition[0].y;
        min[2] = max[2] = mesh.m_rawposition[0].z;
        for (const aiVector3D v : mesh.m_rawposition) {
            min[0] = std::min(min[0], v.x);
            max[0] = std::max(max[0], v.x);
            min[1] = std::min(min[1], v.y);
            max[1] = std::max(max[1], v.y);
            min[2] = std::min(min[2], v.z);
            max[2] = std::max(max[2], v.z);
        }
        fmt::print(stats, "\n========== BOUNDS ==========\n");
        fmt::print(stats, "Bounding box: ({}, {}, {}) ({}, {}, {})\n", min[0],
                   min[1], min[2], max[0], max[1], max[2]);
        float amax[3];
        for (int i = 0; i < 3; i++) {
            amax[i] = std::max(-min[i], max[i]);
        }
        fmt::print(stats, "Absolute bounds: ({}, {}, {})\n", amax[0], amax[1],
                   amax[2]);
        fmt::print(stats, "\n");
    }
    return mesh;
}

void Mesh::Dump(std::FILE *stats) const {
    assert(stats != nullptr);
    fmt::print(stats,
               "Imported model:\n"
               "    Vertexes: {}\n    Triangles: {}\n"
               "    Nodes: {}\n    Bones: {}\n",
               m_vertex.size(), m_triangle.size(), m_node.size(),
               m_bone.size());
}

void Mesh::AddNodes(const aiNode *node, int parent) {
    size_t n = m_node.size();
    if (n > std::numeric_limits<int>::max()) {
        throw std::runtime_error("too many nodes");
    }
    int index = n;
    m_node.emplace_back(parent, std::string(Str(node->mName)));
    Node &nn = m_node.back();
    nn.transform = node->mTransformation;
    auto entry = m_node_names.find(nn.name);
    if (entry == m_node_names.end()) {
        m_node_names.emplace(nn.name, index);
    } else {
        entry->second = -1;
    }
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
        (void)&transform;
        const aiVector3D *posarr = mesh->mVertices;
        for (int i = 0; i < nvert; i++) {
            m_rawposition.at(offset + i) = posarr[i];
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

    if (cfg.animate) {
        for (aiBone **bp = mesh->mBones, **be = bp + mesh->mNumBones; bp != be;
             bp++) {
            const aiBone *bone = *bp;
            std::string bone_name = std::string(Str(bone->mName));
            auto entry = m_node_names.find(bone_name);
            if (entry == m_node_names.end()) {
                throw MeshError(fmt::format("no node for bone, name={}",
                                            util::Quote(bone_name)));
            }
            int node_index = entry->second;
            if (node_index < 0) {
                throw MeshError(fmt::format("multiple nodes for bone, name={}",
                                            util::Quote(bone_name)));
            }
            Bone b{};
            b.node = node_index;
            b.name = bone_name;
            b.offset_matrix = bone->mOffsetMatrix;
            const unsigned num_weights = bone->mNumWeights;
            const aiVertexWeight *weights = bone->mWeights;
            for (unsigned i = 0; i < num_weights; i++) {
                b.vertex.push_back(BoneVertex{
                    offset + static_cast<int>(weights[i].mVertexId),
                    weights[i].mWeight,
                });
            }
            m_bone.push_back(std::move(b));
        }
    }
}

void Mesh::SetVertexPos(const std::vector<aiVector3D> &pos) {
    m_vertexpos.resize(m_vertex.size());
    for (size_t i = 0; i < m_vertex.size(); i++) {
        m_vertexpos.at(i) = QuantizeVector(m_transform * pos.at(i));
    }
}

namespace {

// Interpolate between vectors.
aiVector3D Interpolate(const aiVector3D &a, const aiVector3D &b, double frac) {
    return a * static_cast<float>(1.0 - frac) + b * static_cast<float>(frac);
}

// Interpolate between quaternions.
aiQuaternion Interpolate(const aiQuaternion &a, const aiQuaternion &b,
                         double frac) {
    aiQuaternion r;
    aiQuaternion::Interpolate(r, a, b, static_cast<float>(frac));
    return r;
}

// Read an object from an animation channel.
template <typename Key>
typename Key::elem_type ReadObject(double time, const Key *keys, unsigned count,
                                   typename Key::elem_type default_value) {
    if (count == 0) {
        return default_value;
    }
    unsigned idx = 0;
    while (idx < count && time >= keys[idx].mTime) {
        idx++;
    }
    if (idx == 0) {
        return keys[0].mValue;
    }
    if (idx == count) {
        return keys[idx - 1].mValue;
    }
    const Key &a = keys[idx - 1];
    const Key &b = keys[idx];
    double delta = b.mTime - a.mTime;
    if (!(delta > 1e-5)) {
        return a.mValue;
    }
    double frac = (time - a.mTime) / delta;
    return Interpolate(a.mValue, b.mValue, frac);
}

} // namespace

void Mesh::AddAnimation(const aiAnimation *animation) {
    std::string node_name;
    const double time = animation->mDuration * 1.0;
    int nodecount = m_node.size();
    int vertcount = m_vertex.size();
    std::vector<aiMatrix4x4> local_transform(nodecount);
    for (size_t i = 0; i < local_transform.size(); i++) {
        local_transform[i] = m_node[i].transform;
    }
    std::vector<aiMatrix4x4> global_transform(nodecount);
    std::vector<aiVector3D> vectors(vertcount);

    // Update local transforms.
    for (aiNodeAnim **cp = animation->mChannels,
                    **ce = cp + animation->mNumChannels;
         cp != ce; cp++) {
        aiNodeAnim *chan = *cp;
        node_name = std::string(Str(chan->mNodeName));
        const auto entry = m_node_names.find(node_name);
        if (entry == m_node_names.end()) {
            throw MeshError(fmt::format(
                "animation refers to unknown node, animation={}, node={}",
                util::Quote(Str(animation->mName)), util::Quote(node_name)));
        }
        const int node_index = entry->second;
        if (node_index == -1) {
            throw MeshError(fmt::format(
                "multiple nodes match animation channel, animation={}, node={}",
                util::Quote(Str(animation->mName)), util::Quote(node_name)));
        }
        // Find key after the given point in time.
        const aiVector3D position =
            ReadObject(time, chan->mPositionKeys, chan->mNumPositionKeys,
                       aiVector3D(0.0f));
        aiQuaternion rotation = ReadObject(
            time, chan->mRotationKeys, chan->mNumRotationKeys, aiQuaternion());
        const aiVector3D scaling = ReadObject(
            time, chan->mScalingKeys, chan->mNumScalingKeys, aiVector3D(1.0f));
        local_transform.at(node_index) =
            aiMatrix4x4(scaling, rotation, position);
    }

    // Update global transforms.
    for (int i = 0; i < nodecount; i++) {
        const Node &nn = m_node.at(i);
        aiMatrix4x4 &gmat = global_transform.at(i);
        const aiMatrix4x4 &lmat = local_transform.at(i);
        if (nn.parent == -1) {
            gmat = lmat;
        } else {
            const aiMatrix4x4 &parent = global_transform.at(nn.parent);
            gmat = parent * lmat;
        }
    }

    // Evaluate bones.
    for (aiVector3D &v : vectors) {
        v = aiVector3D();
    }
    for (const Bone &bone : m_bone) {
        aiMatrix4x4 mat = global_transform.at(bone.node) * bone.offset_matrix;
        for (const BoneVertex &v : bone.vertex) {
            vectors.at(v.index) += (mat * m_rawposition.at(v.index)) * v.weight;
        }
    }

    SetVertexPos(vectors);
}

} // namespace modelconvert
