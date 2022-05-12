// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#pragma once

#include <cstddef>
#include <cstdint>

namespace util {

// Byte orders.
enum class Endian {
    Big,
    Little,
};

#ifdef __BYTE_ORDER__
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
constexpr Endian NativeEndian = Endian::Little;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
constexpr Endian NativeEndian = Endian::Big;
#else
#error "unknown endian"
#endif
#else
#error "unknown endian"
#endif

// Return true if this is the native byte order.
constexpr bool IsNative(Endian endian) {
    return endian == NativeEndian;
}

// Return true if this is swapped from the native byte order.
constexpr bool IsSwapped(Endian endian) {
    return endian != NativeEndian;
}

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

// Swap an array of 16-bit elements.
void BSwap16Array(uint16_t *arr, size_t n);

// Swap an array of 16-bit elements.
inline void BSwap16Array(int16_t *arr, size_t n) {
    BSwap16Array(reinterpret_cast<uint16_t *>(arr), n);
}

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
