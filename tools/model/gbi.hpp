#pragma once

#include <array>
#include <cstdint>

namespace modelconvert {

// Size of a vertex.
constexpr uint32_t VtxSize = 16;

// Size of a graphics command.
constexpr uint32_t GfxSize = 8;

// Calculate the address of an object relative to the display list start.
inline uint32_t RSPAddress(uint32_t x) {
    return (1u << 24) | x;
}

struct alignas(8) VtxC {
    std::array<int16_t, 3> pos;
    uint16_t pad;
    std::array<int16_t, 2> texcoord;
    std::array<uint8_t, 4> color;

    // Write to output buffer.
    void Write(uint8_t *ptr) const;
};

struct alignas(8) Gfx {
    uint32_t hi;
    uint32_t lo;

    // Write to buffer.
    void Write(uint8_t *ptr) const;

    static Gfx SPVertex(unsigned v, unsigned n, unsigned v0);
    static Gfx SP1Triangle(std::array<unsigned, 3> v1);
    static Gfx SP2Triangle(std::array<unsigned, 3> v1,
                           std::array<unsigned, 3> v2);
    static Gfx SPEndDisplayList();
    static Gfx DPSetPrimColor(unsigned m, unsigned l,
                              std::array<uint8_t, 4> rgba);
};

} // namespace modelconvert
