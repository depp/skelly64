#pragma once

#include <stdint.h>

namespace util {

inline uint16_t BSwap16(uint16_t x) {
    return __builtin_bswap16(x);
}

inline uint32_t BSwap32(uint32_t x) {
    return __builtin_bswap32(x);
}

inline uint32_t BSwapPutF32(float x) {
    union {
        float f;
        uint32_t i;
    } u;
    u.f = x;
    return BSwap32(u.i);
}

#if __BYTE_ORDER__ && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#elif __BYTE_ORDER__ && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#error "Big endian is unsupported"
#else
#error "Unknown endian"
#endif

} // namespace util
