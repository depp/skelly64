#pragma once

#include <stdint.h>

namespace util {

inline uint32_t PutFloat32(float x) {
    union {
        float f;
        uint32_t i;
    } u;
    u.f = x;
    return u.i;
}

inline uint16_t BSwap16(uint16_t x) {
    return __builtin_bswap16(x);
}

inline uint32_t BSwap32(uint32_t x) {
    return __builtin_bswap32(x);
}

#if __BYTE_ORDER__ && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#elif __BYTE_ORDER__ && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#error "Big endian is unsupported"
#else
#error "Unknown endian"
#endif

// Read a big-endian 16-bit integer.
inline uint16_t Read16(const void *ptr) {
    const uint8_t *p = static_cast<const uint8_t *>(ptr);
    return (static_cast<uint32_t>(p[0]) << 8) | static_cast<uint32_t>(p[1]);
}

// Read a big-endian 32-bit integer.
inline uint32_t Read32(const void *ptr) {
    const uint8_t *p = static_cast<const uint8_t *>(ptr);
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

// Read a big-endian 64-bit integer.
inline uint64_t Read64(const void *ptr) {
    const uint8_t *p = static_cast<const uint8_t *>(ptr);
    return (static_cast<uint64_t>(Read32(p)) << 32) |
           static_cast<uint64_t>(Read32(p + 4));
}

// Write a big-endian 16-bit integer.
inline void Write16(void *ptr, uint16_t v) {
    uint8_t *p = static_cast<uint8_t *>(ptr);
    p[0] = v >> 8;
    p[1] = v;
}

// Write a big-endian 32-bit integer.
inline void Write32(void *ptr, uint32_t v) {
    uint8_t *p = static_cast<uint8_t *>(ptr);
    p[0] = v >> 24;
    p[1] = v >> 16;
    p[2] = v >> 8;
    p[3] = v;
}

// Write a big-endian 64-bit integer.
inline void Write64(void *ptr, uint64_t v) {
    uint8_t *p = static_cast<uint8_t *>(ptr);
    Write32(p, v >> 32);
    Write32(p + 4, v);
}

} // namespace util
