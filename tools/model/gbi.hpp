#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace modelconvert {
namespace gbi {

// Calculate the address of an object relative to the display list start.
inline uint32_t RSPAddress(uint32_t x) {
    return (1u << 24) | x;
}

// Vertex data.
struct alignas(8) Vtx {
    // Size of vertex data.
    static constexpr size_t Size = 16;

    std::array<int16_t, 3> pos;
    uint16_t pad;
    std::array<int16_t, 2> texcoord;
    std::array<uint8_t, 4> color; // Or normal.

    // Write to output buffer.
    void Write(uint8_t *ptr) const;
};

// Offsets within the vertex cache.
enum class VertexField {
    RGBA = 16,
    ST = 20,
    XY = 24,
    Z = 28,
};

// Microcode command.
struct alignas(8) Gfx {
    // Size of microcode command.
    static constexpr size_t Size = 8;

    uint32_t hi;
    uint32_t lo;

    // Write to buffer.
    void Write(uint8_t *ptr) const;

    static Gfx SPVertex(unsigned v, unsigned n, unsigned v0);
    static Gfx SPModifyVertex(int vertex, VertexField field, uint32_t value);
    static Gfx SP1Triangle(std::array<int, 3> v1);
    static Gfx SP2Triangle(std::array<int, 3> v1, std::array<int, 3> v2);
    static Gfx SPEndDisplayList();
    static Gfx DPSetPrimColor(unsigned m, unsigned l,
                              std::array<uint8_t, 4> rgba);
};

} // namespace gbi
} // namespace modelconvert
