// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#pragma once

#include <stdint.h>

namespace util {

// Murmur 3 hasher.
struct Murmur3 {
    uint32_t state;
    uint32_t len;

    static Murmur3 Initial(uint32_t seed) { return Murmur3{seed, 0}; }

    void Update(uint32_t data) {
        uint32_t k = data;
        k *= 0xcc9e2d51u;
        k = (k << 15) | (k >> 17);
        k *= 0x1b873593u;
        state = state ^ k;
        state = (state << 13) | (state >> 19);
        state = (state * 5) + 0xe6546b64u;
        len += 4;
    }

    uint32_t Hash() const {
        uint32_t hash = state;
        hash ^= len;
        hash ^= hash >> 16;
        hash *= 0x85ebca6bu;
        hash ^= hash >> 13;
        hash *= 0xc2b2ae35u;
        hash ^= hash >> 16;
        return hash;
    }
};

} // namespace util
