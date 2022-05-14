// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "tools/vadpcm/stats.hpp"

namespace vadpcm {

Stats Stats::Calculate(size_t sample_count, const int16_t *ref,
                       const int16_t *out) {
    double xsum = 0.0, xc = 0.0, ysum = 0.0, yc = 0.0;
    for (size_t i = 0; i < sample_count; i++) {
        double x = ref[i] * (1.0 / 32768.0);
        double y = (out[i] - ref[i]) * (1.0 / 32768.0);
        x = x * x;
        y = y * y;

        // Kahan sum.
        double a, t;
        a = x - xc;
        t = xsum + a;
        xc = (t - xsum) - a;
        xsum = t;
        a = y - yc;
        t = ysum + a;
        yc = (t - ysum) - a;
        ysum = t;
    }
    double n = sample_count;
    Stats s;
    s.signal = xsum / n;
    s.noise = ysum / n;
    return s;
}

} // namespace vadpcm
