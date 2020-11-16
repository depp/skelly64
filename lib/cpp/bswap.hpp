#pragma once

#include <stdint.h>

namespace util {

inline uint16_t BSwap16(uint16_t x) {
    return __builtin_bswap16(x);
}

inline uint32_t BSwap32(uint32_t x) {
    return __builtin_bswap32(x);
}

} // namespace util
