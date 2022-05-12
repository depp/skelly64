// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "lib/cpp/bswap.hpp"

namespace util {

void BSwap16Array(uint16_t *arr, size_t n) {
    for (size_t i = 0; i < n; i++) {
        arr[i] = BSwap16(arr[i]);
    }
}

} // namespace util
