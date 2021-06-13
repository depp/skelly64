#include "tools/model/gbi.hpp"

#include "lib/cpp/bswap.hpp"

#include <cstring>
#include <stdexcept>

#include <fmt/format.h>

namespace modelconvert {
namespace gbi {

namespace {

uint32_t ShiftL(unsigned v, unsigned s, unsigned w) {
    return (v & ((1u << w) - 1)) << s;
}

uint32_t UnshiftL(unsigned v, unsigned s, unsigned w) {
    return (v >> s) & ((1u << w) - 1);
}

// Pack the indexes of a triangle into a single word.
uint32_t Triangle(std::array<int, 3> v) {
    return ShiftL(v[0] * 2, 16, 8) | ShiftL(v[1] * 2, 8, 8) |
           ShiftL(v[2] * 2, 0, 8);
}

// Unpack the indexes of a triangle from a single word.
std::array<int, 3> UnTriangle(uint32_t v) {
    int x = UnshiftL(v, 16, 8);
    int y = UnshiftL(v, 8, 8);
    int z = UnshiftL(v, 0, 8);
    return {{x, y, z}};
}

// Pack RGBA into a single word.
uint32_t RGBA(std::array<uint8_t, 4> rgba) {
    return ShiftL(rgba[0], 24, 8) | ShiftL(rgba[1], 16, 8) |
           ShiftL(rgba[2], 8, 8) | ShiftL(rgba[3], 0, 8);
}

// Unpack RGBA from a single word.
std::array<uint8_t, 4> UnRGBA(uint32_t v) {
    uint8_t r = UnshiftL(v, 24, 8);
    uint8_t g = UnshiftL(v, 16, 8);
    uint8_t b = UnshiftL(v, 8, 8);
    uint8_t a = UnshiftL(v, 0, 8);
    return {{r, g, b, a}};
}

// GBI opcodes.
enum {
    G_VTX = 0x01,
    G_MODIFYVTX = 0x02,
    G_TRI1 = 0x05,
    G_TRI2 = 0x06,
    G_ENDDL = 0xdf,
    G_SETPRIMCOLOR = 0xfa,
};

// Return the field
const char *FieldName(VertexField f) {
    switch (f) {
    case VertexField::RGBA:
        return "RGBA";
    case VertexField::ST:
        return "ST";
    case VertexField::XY:
        return "XYSCREEN";
    case VertexField::Z:
        return "ZSCREEN";
    default:
        return nullptr;
    }
}

} // namespace

using util::BSwap16;
using util::BSwap32;

void Vtx::WriteBinary(uint8_t *ptr) const {
    static_assert(sizeof(Vtx) == Size);
    Vtx v;
    for (int i = 0; i < 3; i++) {
        v.pos[i] = BSwap16(pos[i]);
    }
    v.pad = 0;
    for (int i = 0; i < 2; i++) {
        v.texcoord[i] = BSwap16(texcoord[i]);
    }
    v.color = color;
    std::memcpy(ptr, &v, sizeof(v));
}

void Vtx::WriteSource(std::vector<uint8_t> *out) const {
    fmt::format_to(std::back_inserter(*out),
                   "{{{{{{{}, {}, {}}}, 0, {{{}, {}}}, {{{}, {}, {}, {}}}}}}}",
                   pos[0], pos[1], pos[2], texcoord[0], texcoord[1], color[0],
                   color[1], color[2], color[3]);
}

void Gfx::WriteBinary(uint8_t *ptr) const {
    static_assert(sizeof(Gfx) == Size);
    Gfx g;
    g.hi = BSwap32(hi);
    g.lo = BSwap32(lo);
    memcpy(ptr, &g, sizeof(g));
}

void Gfx::WriteSource(std::vector<uint8_t> *out) const {
    uint32_t opcode = hi >> 24;
    switch (opcode) {
    case G_VTX: {
        uint32_t n = UnshiftL(hi, 12, 8);
        uint32_t v0 = UnshiftL(hi, 1, 7) - n;
        uint32_t v = lo;
        fmt::format_to(std::back_inserter(*out), "gsSPVertex(0x{:x}, {}, {})",
                       v, n, v0);
    } break;
    case G_MODIFYVTX: {
        uint32_t field = UnshiftL(hi, 16, 8);
        uint32_t vpos = UnshiftL(hi, 0, 16) / 2;
        uint32_t value = lo;
        const char *fieldname = FieldName(static_cast<VertexField>(field));
        if (fieldname == nullptr) {
            throw std::invalid_argument(fmt::format(
                "Gfx::WriteSource: unknown vertex field: {}", field));
        }
        fmt::format_to(std::back_inserter(*out),
                       "gsSPModifyVertex({}, G_MWO_POINT_{}, {})", vpos,
                       fieldname, value);
    } break;
    case G_TRI1: {
        std::array<int, 3> tri = UnTriangle(hi);
        fmt::format_to(std::back_inserter(*out), "gsSP1Triangle({}, {}, {}, 0)",
                       tri[0], tri[1], tri[1]);
    } break;
    case G_TRI2: {
        std::array<int, 3> tris[2] = {UnTriangle(hi), UnTriangle(lo)};
        fmt::format_to(std::back_inserter(*out),
                       "gsSP2Triangles({}, {}, {}, 0, {}, {}, {}, 0)",
                       tris[0][0], tris[0][1], tris[0][2], tris[1][0],
                       tris[1][1], tris[1][2]);
    } break;
    case G_ENDDL:
        fmt::format_to(std::back_inserter(*out), "gsSPEndDisplayList()");
        break;
    case G_SETPRIMCOLOR: {
        uint32_t m = UnshiftL(hi, 8, 8);
        uint32_t l = UnshiftL(hi, 0, 8);
        std::array<uint8_t, 4> rgba = UnRGBA(lo);
        fmt::format_to(std::back_inserter(*out),
                       "gsDPSetPrimColor({}, {}, {}, {}, {}, {})", m, l,
                       rgba[0], rgba[1], rgba[2], rgba[3]);
    } break;
    default:
        throw std::invalid_argument(
            fmt::format("Gfx::WriteSource: unknown opcode 0x{:02x}", opcode));
    }
}

Gfx Gfx::SPVertex(unsigned v, unsigned n, unsigned v0) {
    return Gfx{
        ShiftL(G_VTX, 24, 8) | ShiftL(n, 12, 8) | ShiftL(v0 + n, 1, 7),
        v,
    };
}

Gfx Gfx::SPModifyVertex(int vertex, VertexField field, uint32_t value) {
    return Gfx{
        ShiftL(G_MODIFYVTX, 24, 8) |
            ShiftL(static_cast<uint32_t>(field), 16, 8) |
            ShiftL(vertex * 2, 0, 16),
        value,
    };
}

Gfx Gfx::SP1Triangle(std::array<int, 3> v1) {
    return Gfx{
        ShiftL(G_TRI1, 24, 8) | Triangle(v1),
        0,
    };
}

Gfx Gfx::SP2Triangle(std::array<int, 3> v1, std::array<int, 3> v2) {
    return Gfx{
        ShiftL(G_TRI2, 24, 8) | Triangle(v1),
        Triangle(v2),
    };
}

Gfx Gfx::SPEndDisplayList() {
    return Gfx{ShiftL(G_ENDDL, 24, 8), 0};
}

Gfx Gfx::DPSetPrimColor(unsigned m, unsigned l, std::array<uint8_t, 4> rgba) {
    return Gfx{
        ShiftL(G_SETPRIMCOLOR, 24, 8) | ShiftL(m, 8, 8) | ShiftL(l, 0, 8),
        RGBA(rgba),
    };
}

} // namespace gbi
} // namespace modelconvert
