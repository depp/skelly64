#pragma once

#include <cstdint>

namespace util {

inline uint32_t Pack16x2(uint16_t hi, uint16_t lo) {
    return (static_cast<uint32_t>(hi) << 16) | lo;
}

inline uint32_t Pack16x2(std::array<uint16_t, 2> a) {
    return Pack16x2(a[0], a[1]);
}

inline uint32_t Pack16x2(std::array<int16_t, 2> a) {
    return Pack16x2(a[0], a[1]);
}

inline uint32_t Pack8x4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) |
           (static_cast<uint32_t>(c) << 8) | static_cast<uint32_t>(d);
}

inline uint32_t Pack8x4(std::array<uint8_t, 4> a) {
    return Pack8x4(a[0], a[1], a[2], a[3]);
}

} // namespace util
