#include "tools/modelconvert/gbi.hpp"

#include "tools/util/bswap.hpp"

#include <cstring>

namespace modelconvert {

using util::BSwap16;
using util::BSwap32;

void VtxC::Write(uint8_t *ptr) const {
    static_assert(sizeof(VtxC) == VtxSize);
    VtxC v;
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

void Gfx::Write(uint8_t *ptr) const {
    static_assert(sizeof(Gfx) == GfxSize);
    Gfx g;
    g.hi = BSwap32(hi);
    g.lo = BSwap32(lo);
    memcpy(ptr, &g, sizeof(g));
}

namespace {

uint32_t ShiftL(unsigned v, unsigned s, unsigned w) {
    return (v & ((1u << w) - 1)) << s;
}

uint32_t Triangle(std::array<unsigned, 3> v) {
    return ShiftL(v[0] * 2, 16, 8) | ShiftL(v[1] * 2, 8, 8) |
           ShiftL(v[2] * 2, 0, 8);
}

} // namespace

enum {
    G_VTX = 0x01,
    G_TRI1 = 0x05,
    G_TRI2 = 0x06,
    G_ENDDL = 0xdf,
    G_SETPRIMCOLOR = 0xfa,
};

Gfx Gfx::SPVertex(unsigned v, unsigned n, unsigned v0) {
    return Gfx{
        ShiftL(G_VTX, 24, 8) | ShiftL(n, 12, 8) | ShiftL(v0 + n, 1, 7),
        v,
    };
}

Gfx Gfx::SP1Triangle(std::array<unsigned, 3> v1) {
    return Gfx{
        ShiftL(G_TRI1, 24, 8) | Triangle(v1),
        0,
    };
}

Gfx Gfx::SP2Triangle(std::array<unsigned, 3> v1, std::array<unsigned, 3> v2) {
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
        ShiftL(rgba[0], 24, 8) | ShiftL(rgba[1], 16, 8) |
            ShiftL(rgba[2], 8, 8) | ShiftL(rgba[3], 0, 8),
    };
}

} // namespace modelconvert
