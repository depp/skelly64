#include "tools/modelconvert/vertex.hpp"

#include "tools/util/hash.hpp"

namespace modelconvert {

bool BoneWeight::operator==(const BoneWeight &w) const {
    return bone == w.bone && weight == w.weight;
}

bool BoneWeight::operator!=(const BoneWeight &w) const {
    return !(*this == w);
}

bool Vertex::operator==(const Vertex &v) const {
    return pos == v.pos && texcoord == v.texcoord && color == v.color &&
           alpha == v.alpha && normal == v.normal &&
           bone_weights == v.bone_weights;
}

bool Vertex::operator!=(const Vertex &v) const {
    return !(*this == v);
}

namespace {

uint32_t pack32(uint16_t hi, uint16_t lo) {
    return (hi << 16) | lo;
}

uint16_t pack16(uint8_t hi, uint8_t lo) {
    return (hi << 8) | lo;
}

} // namespace

uint32_t Vertex::Hash() const {
    util::Murmur3 h = util::Murmur3::Initial(0);
    h.Update(pack32(pos[0], pos[1]));
    h.Update(pos[2]);
    h.Update(pack32(texcoord[0], texcoord[1]));
    h.Update(alpha);
    h.Update(pack32(pack16(color[0], color[1]), pack16(color[2], 0)));
    h.Update(pack32(pack16(normal[0], normal[1]), pack16(normal[2], 0)));
    for (size_t i = 0; i < MaxBones; i++) {
        h.Update(bone_weights[i].bone);
        h.Update(bone_weights[i].weight);
    }
    return h.Hash();
}

} // namespace modelconvert
