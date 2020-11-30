#include "tools/modelconvert/mesh.hpp"

#include "tools/util/hash.hpp"

#include <fmt/core.h>

namespace modelconvert {

Axes Axes::Default() {
    return Axes{{{false, false, false}}, {{0, 1, 2}}};
}

std::string Axes::ToString() const {
    std::string r;
    for (int i = 0; i < 3; i++) {
        if (i != 0) {
            r.push_back(',');
        }
        if (negate[i]) {
            r.push_back('-');
        }
        const int n = index[i];
        if (n < 0 || n >= 3) {
            throw std::logic_error("invalid Axes");
        }
        r.push_back("XYZ"[n]);
    }
    return r;
}

namespace {

bool IsWhite(char c) {
    return c == ' ' || c == '\t';
}

const char *SkipWhite(const char *ptr, const char *end) {
    while (ptr != end && IsWhite(*ptr)) {
        ptr++;
    }
    return ptr;
}

} // namespace

Axes Axes::Parse(std::string_view s) {
    const char *ptr = s.begin(), *end = s.end();
    Axes r;
    for (int i = 0; i < 3; i++) {
        ptr = SkipWhite(ptr, end);
        if (i != 0) {
            if (ptr != end && (*ptr == ':' || *ptr == ',')) {
                ptr++;
            }
        }
        ptr = SkipWhite(ptr, end);
        if (ptr == end) {
            throw std::invalid_argument("not enough components");
        }
        bool negate = false;
        if (ptr != end && *ptr == '-') {
            negate = true;
            ptr++;
        }
        r.negate[i] = negate;
        ptr = SkipWhite(ptr, end);
        if (ptr == end) {
            throw std::invalid_argument("bad axis");
        }
        int index;
        switch (*ptr) {
        case 'x':
        case 'X':
            index = 0;
            break;
        case 'y':
        case 'Y':
            index = 1;
            break;
        case 'z':
        case 'Z':
            index = 2;
            break;
        default:
            throw std::invalid_argument("bad axis");
        }
        ptr++;
        r.index[i] = index;
        for (int j = 0; j < i; j++) {
            if (r.index[j] == index) {
                std::string msg =
                    fmt::format("duplicate axis: {}", "XYZ"[index]);
                throw std::invalid_argument(msg);
            }
        }
    }
    ptr = SkipWhite(ptr, end);
    if (ptr != end) {
        throw std::invalid_argument("extra data after axes");
    }
    return r;
}

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

unsigned VertexSet::Add(const FVertex &v) {
    auto it = indexes.find(v.vert);
    if (it != indexes.end()) {
        return it->second;
    }
    unsigned index = vertexes.size();
    vertexes.emplace_back(v);
    indexes.emplace(v.vert, index);
    return index;
}

} // namespace modelconvert
