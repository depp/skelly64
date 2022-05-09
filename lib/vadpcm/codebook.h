// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#pragma once
// VADPCM codebook data structures. Internal header.

#include <stdalign.h>

enum {
    // The number of samples in a VADPCM vector. This is likely chosen to equal
    // the number of 16-bit samples that can fit in a vector register in the
    // Nintendo 64 Reality Signal Processor, which has 128-bit vector registers.
    // Conveniently, this is a common size for modern SIMD architectures.
    kVADPCMVectorSampleCount = 8,
};

// A vector of sample data.
struct vadpcm_vector {
    alignas(16) short v[kVADPCMVectorSampleCount];
};

struct vadpcm_codebook {
    // The number of predictor coefficient sets in the codebook. Must be
    // positive. The most common value seen here is 4.
    int predictor_count;

    // The number of predictor coefficients in each set. Only the number 2 has
    // been observed here.
    int order;

    // The predictors.
    struct vadpcm_vector data[];
};

// Return the vectors for the given set of predictor coefficients.
inline const struct vadpcm_vector *vadpcm_codebook_get(
    const struct vadpcm_codebook *b, int predictor) {
    return b->data + predictor * b->order;
}

// Allocate a vadpcm codebook with the given size. Returns NULL if out of
// memory. The result must be freed with free().
struct vadpcm_codebook *vadpcm_codebook_new(int predictor_count, int order);
