// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#pragma once

#include <cstddef>
#include <cstdint>

namespace vadpcm {

struct Stats {
    // Mean square signal level, where 1.0 is full scale.
    double signal;

    // Mean square noise level, where 1.0 is full scale.
    double noise;

    // Calculate the statistics for a signal and the encoded signal.
    static Stats Calculate(size_t sample_count, const int16_t *ref,
                           const int16_t *out);
};

} // namespace vadpcm
