// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "lib/vadpcm/codebook.h"
#include "lib/vadpcm/binary.h"
#include "lib/vadpcm/vadpcm.h"

#include <stdlib.h>

enum {
    // The only supported version number for VADPCM data. This is also the only
    // known version number.
    kVADPCMVersion = 1,
};

// Emit inline functions.
const struct vadpcm_vector *vadpcm_codebook_get(const struct vadpcm_codebook *b,
                                                int predictor);

struct vadpcm_codebook *vadpcm_codebook_new(int predictor_count, int order) {
    struct vadpcm_codebook *b = malloc(
        sizeof(*b) + sizeof(struct vadpcm_vector) * predictor_count * order);
    if (b == NULL) {
        return NULL;
    }
    b->predictor_count = predictor_count;
    b->order = order;
    return b;
}

vadpcm_error vadpcm_read_codebook_aifc(struct vadpcm_codebook **b,
                                       const void *data, size_t size) {
    const uint8_t *restrict p = data;

    // Read the header:
    // u16 version (equals 1)
    // u16 order
    // u16 predictor count
    if (size < 2) {
        return kVADPCMErrInvalidData;
    }
    int version = vadpcm_read16(p);
    if (version != kVADPCMVersion) {
        return kVADPCMErrUnknownVersion;
    }
    int order = vadpcm_read16(p + 2);
    int predictor_count = vadpcm_read16(p + 4);
    if (order == 0 || predictor_count == 0) {
        return kVADPCMErrInvalidData;
    }
    if (order > kVADPCMMaxOrder) {
        return kVADPCMErrLargeOrder;
    }

    // Check that the buffer is the large enough to contain all the predictor
    // data. Note that the values in the computation are bounded and won't
    // overflow.
    p += 6;
    size_t vcount = (size_t)predictor_count * order;
    size_t psize = size - 6;
    if (psize < vcount * 16) {
        return kVADPCMErrInvalidData;
    }

    // Read the predictor data.
    struct vadpcm_codebook *bptr = vadpcm_codebook_new(predictor_count, order);
    if (bptr == NULL) {
        return kVADPCMErrNoMemory;
    }
    for (size_t i = 0; i < vcount; i++) {
        for (int j = 0; j < 8; j++) {
            bptr->data[i].v[j] = vadpcm_read16(p + 16 * i + 2 * j);
        }
    }
    *b = bptr;
    return 0;
}
