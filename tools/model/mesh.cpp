#include "tools/modelconvert/mesh.hpp"

#include "tools/modelconvert/config.hpp"
#include "tools/util/hash.hpp"
#include "tools/util/pack.hpp"
#include "tools/util/quote.hpp"

#include <assimp/scene.h>
#include <cassert>
#include <fmt/core.h>
#include <unordered_map>

namespace modelconvert {

namespace {

std::string_view Str(const aiString &s) {
    return std::string_view(s.data, s.length);
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

void QuantizeVectors(std::vector<std::array<int16_t, 3>> *out,
                     const aiVector3D *vs, int vscount,
                     const aiMatrix4x4 &transform) {
    out->reserve(out->size() + vscount);
    for (int i = 0; i < vscount; i++) {
        out->push_back(QuantizeVector(transform * vs[i]));
    }
}

// A single vertex under the influence of a bone.
struct BoneVertex {
    int index;
    float weight;
};

struct Bone {
    int node;
    std::string name;
    std::vector<BoneVertex> vertex;
    aiMatrix4x4 offset_matrix; // mesh space -> bone space
};

// Information about a node in the hierarchy.
struct Node {
    Node(int parent, std::string name)
        : parent{parent}, name{std::move(name)} {}

    int parent;
    std::string name;
    aiMatrix4x4 transform; // Node's transformation relative to parent.

    // Temporary matrixes for evaluating animations.
    aiMatrix4x4 current_local;
    aiMatrix4x4 current_global;
};

struct FrameData {
    // Hash of position data.
    uint32_t hash;

    // Position data.
    std::vector<std::array<int16_t, 3>> position;
};

// Bounding box.
class Bounds {
public:
    Bounds() : m_min{1.0f, 1.0f, 1.0f}, m_max{-1.0, -1.0f, -1.0f} {}

    void Add(const aiVector3D *vs, unsigned vscount,
             const aiMatrix4x4 &transform) {
        if (vscount == 0) {
            return;
        }
        if (m_min[0] > m_max[0]) {
            aiVector3D v = transform * vs[0];
            m_min[0] = m_max[0] = v.x;
            m_min[1] = m_max[1] = v.y;
            m_min[2] = m_max[2] = v.z;
        }
        for (unsigned i = 0; i < vscount; i++) {
            aiVector3D v = transform * vs[i];
            m_min[0] = std::min(m_min[0], v.x);
            m_min[1] = std::min(m_min[1], v.y);
            m_min[2] = std::min(m_min[2], v.z);
            m_max[0] = std::max(m_max[0], v.x);
            m_max[1] = std::max(m_max[1], v.y);
            m_max[2] = std::max(m_max[2], v.z);
        }
    }

    std::string ToString() const {
        return fmt::format("({}, {}, {}) ({}, {}, {})", m_min[0], m_min[1],
                           m_min[2], m_max[0], m_max[1], m_max[2]);
    }

private:
    std::array<float, 3> m_min;
    std::array<float, 3> m_max;
};

// AssImp mesh importer.
class Importer {
public:
    Importer(const Config &cfg, std::FILE *stats)
        : m_cfg{cfg}, m_stats{stats} {}

    void Import(const aiScene *scene);

    // Create the mesh. Destroys the importer.
    Mesh IntoMesh();

private:
    // Get the model bounds.
    void GetBounds(Bounds *bounds, const aiScene *scene, const aiNode *node,
                   const aiMatrix4x4 &transform);

    // Add a node and all its children recursively, given the index of the
    // parent.
    void AddNodes(const aiNode *node, int parent);

    // Add the meshes belonging to a node and its children.
    void AddMeshes(const aiScene *scene, const aiNode *node,
                   const aiMatrix4x4 &transform);

    // Add a AssImp mesh.
    void AddMesh(const aiMesh *mesh, const aiMatrix4x4 &transform);

    // Add an animation to the mesh.
    void AddAnimation(int index, const aiAnimation *animation);

    // Create a frame of animation. Returns the index of the new frame.
    int CreateFrame(const aiAnimation *animation, double time);

    // Add a frame of animation, given the position data. Returns the index of
    // the new frame.
    int AddFrame(std::vector<std::array<int16_t, 3>> &&position);

    const Config &m_cfg;
    std::FILE *m_stats;

    // Model transformation.
    aiMatrix4x4 m_transform;

    // Vertex data.
    std::vector<aiVector3D> m_rawposition;  // Untransformed.
    std::vector<aiVector3D> m_tempposition; // Bones.
    std::vector<VertexAttr> m_vertex;

    // Triangles.
    std::vector<Triangle> m_triangle;

    // Bones and nodes.
    std::unordered_map<std::string, int> m_node_names; // -1 = multiple.
    std::vector<Bone> m_bone;
    std::vector<Node> m_node;

    // Quantized vertex positions.
    std::vector<std::array<int16_t, 3>> m_vertexpos; // Transformed.

    // Animations.
    std::vector<std::unique_ptr<Animation>> m_animation;

    // Frame data.
    std::vector<FrameData> m_frame;
};

void Importer::Import(const aiScene *scene) {
    aiMatrix4x4 axes = m_cfg.axes.ToMatrix();
    if (m_stats) {
        Bounds bounds;
        GetBounds(&bounds, scene, scene->mRootNode, axes);
        fmt::print(m_stats, "Model bounds: {}\n", bounds.ToString());
    }
    m_transform = axes * m_cfg.scale;
    AddNodes(scene->mRootNode, -1);
    AddMeshes(scene, scene->mRootNode, aiMatrix4x4());
    if (m_rawposition.empty() || m_vertexpos.empty()) {
        throw MeshError("empty mesh");
    }
    {
        int frame = AddFrame(std::move(m_vertexpos));
        if (frame != 0) {
            // Assertion.
            throw std::runtime_error("bind pose is not frame 0");
        }
    }
    if (m_cfg.animate) {
        int animcount = scene->mNumAnimations;
        for (int i = 0; i < animcount; i++) {
            AddAnimation(i, scene->mAnimations[i]);
        }
    }
    if (m_stats) {
        fmt::print(m_stats, "\n========== Model Stats ==========\n");
        fmt::print(m_stats, "Vertexes: {}\n", m_vertex.size());
        fmt::print(m_stats, "Triangles: {}\n", m_triangle.size());
        fmt::print(m_stats, "Nodes: {}\n", m_node.size());
        fmt::print(m_stats, "Bones: {}\n", m_bone.size());
        fmt::print(m_stats, "\n");
    }
}

Mesh Importer::IntoMesh() {
    Mesh mesh;
    mesh.vertex = std::move(m_vertex);
    mesh.triangle = std::move(m_triangle);
    mesh.animation = std::move(m_animation);
    if (m_frame.empty()) {
        throw std::runtime_error("no frames");
    }
    mesh.animation_frame.reserve(m_frame.size());
    for (FrameData &frame : m_frame) {
        mesh.animation_frame.emplace_back(std::move(frame.position));
    }
    return mesh;
}

void Importer::GetBounds(Bounds *bounds, const aiScene *scene,
                         const aiNode *node,
                         const aiMatrix4x4 &parent_transform) {
    aiMatrix4x4 transform = parent_transform * node->mTransformation;
    for (const unsigned *mp = node->mMeshes, *me = mp + node->mNumMeshes;
         mp != me; mp++) {
        const aiMesh *mesh = scene->mMeshes[*mp];
        bounds->Add(mesh->mVertices, mesh->mNumVertices, transform);
    }
    for (aiNode **np = node->mChildren, **ne = np + node->mNumChildren;
         np != ne; np++) {
        GetBounds(bounds, scene, *np, transform);
    }
}

void Importer::AddNodes(const aiNode *node, int parent) {
    size_t n = m_node.size();
    if (n > std::numeric_limits<int>::max()) {
        throw MeshError("too many nodes");
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

void Importer::AddMeshes(const aiScene *scene, const aiNode *node,
                         const aiMatrix4x4 &transform) {
    const aiMatrix4x4 node_transform = transform * node->mTransformation;
    for (const unsigned *mp = node->mMeshes, *me = mp + node->mNumMeshes;
         mp != me; mp++) {
        unsigned mesh_id = *mp;
        if (mesh_id >= scene->mNumMeshes) {
            throw MeshError("bad mesh reference in scene");
        }
        AddMesh(scene->mMeshes[mesh_id], node_transform);
    }
    for (aiNode **cp = node->mChildren, **ce = cp + node->mNumChildren;
         cp != ce; cp++) {
        AddMeshes(scene, *cp, node_transform);
    }
}

void Importer::AddMesh(const aiMesh *mesh, const aiMatrix4x4 &transform) {
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
            m_rawposition.at(offset + i) = posarr[i];
        }
        QuantizeVectors(&m_vertexpos, posarr, nvert, m_transform * transform);
    }

    // Get texture coordinates.
    if (m_cfg.use_texcoords) {
        const aiVector3D *texcoordarr = mesh->mTextureCoords[0];
        if (texcoordarr == nullptr) {
            if (m_stats != nullptr) {
                fmt::print(m_stats, "No texture coordinates\n");
            }
            goto done_texcoords;
        }
        if (m_cfg.texcoord_bits < 0 || m_cfg.texcoord_bits >= 32) {
            throw std::range_error("texcoord_bits out of range");
        }
        const float scale = 1 << m_cfg.texcoord_bits;
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
    if (m_cfg.use_vertex_colors) {
        const aiColor4D *colorarr = mesh->mColors[0];
        if (colorarr == nullptr) {
            if (m_stats != nullptr) {
                fmt::print(m_stats, "No colors\n");
            }
            goto done_colors;
        }
        for (int i = 0; i < nvert; i++) {
            m_vertex.at(offset + i).color = ImportColor(colorarr[i]);
        }
    }
done_colors:

    // Get vertex normals.
    if (m_cfg.use_normals) {
        const aiVector3D *normarr = mesh->mNormals;
        if (normarr == nullptr) {
            if (m_stats != nullptr) {
                fmt::print(m_stats, "No normals\n");
            }
            goto done_normals;
        }
        for (int i = 0; i < nvert; i++) {
            std::array<float, 3> fnorm = ImportVector(normarr[i]);
            fnorm = m_cfg.axes.Apply(fnorm);
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

    if (m_cfg.animate) {
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

void Importer::AddAnimation(int index, const aiAnimation *animation) {
    const double duration = animation->mDuration;
    int framecount = std::lrint(duration + 1.0);
    std::unique_ptr<Animation> anim = std::make_unique<Animation>();
    anim->duration = duration;
    if (framecount <= 1) {
        AnimationFrame frame{};
        frame.data_index = CreateFrame(animation, 0.0);
        anim->frame.push_back(frame);
    } else if (framecount > 100) {
        throw MeshError("too maniy frames in animation");
    } else {
        anim->frame.reserve(framecount);
        for (int i = 0; i < framecount; i++) {
            double time = i * (duration / (framecount - 1));
            AnimationFrame frame{};
            frame.time = time;
            frame.data_index = CreateFrame(animation, time);
            anim->frame.push_back(frame);
        }
    }
    if (static_cast<size_t>(index) >= m_animation.size()) {
        m_animation.resize(index + 1);
    }
    std::unique_ptr<Animation> &slot = m_animation.at(index);
    if (slot) {
        throw MeshError("multiple animations in same slot");
    }
    slot = std::move(anim);
}

int Importer::CreateFrame(const aiAnimation *animation, double time) {
    int vertcount = m_vertex.size();

    // Reset local transforms.
    for (Node &node : m_node) {
        node.current_local = node.transform;
    }

    // Update local transforms from animation channels.
    std::string node_name;
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
        const aiVector3D position =
            ReadObject(time, chan->mPositionKeys, chan->mNumPositionKeys,
                       aiVector3D(0.0f));
        aiQuaternion rotation = ReadObject(
            time, chan->mRotationKeys, chan->mNumRotationKeys, aiQuaternion());
        const aiVector3D scaling = ReadObject(
            time, chan->mScalingKeys, chan->mNumScalingKeys, aiVector3D(1.0f));
        m_node.at(node_index).current_local =
            aiMatrix4x4(scaling, rotation, position);
    }

    // Update global transforms.
    for (Node &node : m_node) {
        if (node.parent == -1) {
            node.current_global = node.current_local;
        } else {
            // Parent index is always < node index.
            const aiMatrix4x4 &parent = m_node.at(node.parent).current_global;
            node.current_global = parent * node.current_local;
        }
    }

    // Evaluate bones.
    m_tempposition.resize(vertcount);
    for (aiVector3D &v : m_tempposition) {
        v = aiVector3D();
    }
    for (const Bone &bone : m_bone) {
        aiMatrix4x4 mat =
            m_node.at(bone.node).current_global * bone.offset_matrix;
        for (const BoneVertex &v : bone.vertex) {
            m_tempposition.at(v.index) +=
                (mat * m_rawposition.at(v.index)) * v.weight;
        }
    }

    m_vertexpos.clear();
    QuantizeVectors(&m_vertexpos, m_tempposition.data(), vertcount,
                    m_transform);
    return AddFrame(std::move(m_vertexpos));
}

int Importer::AddFrame(std::vector<std::array<int16_t, 3>> &&position) {
    util::Murmur3 hash_state = util::Murmur3::Initial(0);
    for (const std::array<int16_t, 3> &pos : position) {
        hash_state.Update(util::Pack16x2(pos[0], pos[1]));
        hash_state.Update(pos[2]);
    }
    uint32_t hash = hash_state.Hash();
    auto start = m_frame.begin(), end = m_frame.end();
    for (auto ptr = start; ptr != end; ++ptr) {
        FrameData &frame = *ptr;
        if (frame.hash == hash && frame.position == position) {
            int index = ptr - start;
            if (m_stats) {
                fmt::print(m_stats, "Reusing frame {}\n", index);
            }
            return index;
        }
    }
    int index = m_frame.size();
    FrameData &frame = m_frame.emplace_back();
    frame.hash = hash;
    frame.position = std::move(position);
    return index;
}

} // namespace

Mesh Mesh::Import(const Config &cfg, std::FILE *stats, const aiScene *scene) {
    Importer imp{cfg, stats};
    imp.Import(scene);
    return imp.IntoMesh();
}

} // namespace modelconvert
