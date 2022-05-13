// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#pragma once
// VADPCM encoding and decoding.

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#define VADPCM_RESTRICT
extern "C" {
#else
#define VADPCM_RESTRICT restrict
#endif

// Library error codes.
typedef enum {
    // No error (success). Equal to 0.
    kVADPCMErrNone,

    // Invalid data.
    kVADPCMErrInvalidData,

    // Predictor order is too large.
    kVADPCMErrLargeOrder,

    // Predictor count is too large.
    kVADPCMErrLargePredictorCount,

    // Data uses an unsupported / unknown version of VADPCM.
    kVADPCMErrUnknownVersion,
} vadpcm_error;

// Return the short name of the VADPCM error code. Returns NULL for unknown
// error codes.
const char *vadpcm_error_name(vadpcm_error err);

enum {
    // The number of samples in a VADPCM frame.
    kVADPCMFrameSampleCount = 16,

    // The number of bytes in an encoded VADPCM frame.
    kVADPCMFrameByteSize = 9,

    // The maximum supported predictor order. Do not change this value.
    kVADPCMMaxOrder = 8,

    // The maximum supported predictor count. This is determined by the frame
    // format, which uses four bits to store the predictor number.
    kVADPCMMaxPredictorCount = 16,

    // The number of samples in a VADPCM vector. This is likely chosen to equal
    // the number of 16-bit samples that can fit in a vector register in the
    // Nintendo 64 Reality Signal Processor, which has 128-bit vector registers.
    // Conveniently, this is a common size for modern SIMD architectures.
    kVADPCMVectorSampleCount = 8,

    // The predictor order when encoding. Other values are not supported. Do not
    // change this value.
    kVADPCMEncodeOrder = 2,
};

// A vector of sample data.
struct vadpcm_vector {
    alignas(16) short v[kVADPCMVectorSampleCount];
};

// Specification for a codebook.
//
// The number of vectors in a codebook is equal to the predictor count
// multiplied by the order.
struct vadpcm_codebook_spec {
    // The number of predictor coefficient sets in the codebook. Must be
    // positive. The most common value seen here is 4.
    int predictor_count;

    // The number of predictor coefficients in each set. Only the number 2 has
    // been observed here.
    int order;
};

// Parse a codebook spec, as it appears in an AIFC file. On success, fills in
// 'spec' and stores the offset to the vector data in data_offset.
//
// The data is taken from an AIFC 'APPL' (application-specific) chunk with the
// name "VADPCMCODES". The chunk header, APPL header, and chunk name should not
// be included in the data passed to this function.
//
// Error codes:
//   kVADPCMErrInvalidData: Order or predictor count is zero, or the data is
//                          incomplete (unexpected EOF).
//   kVADPCMErrLargeOrder: Order is larger than largest supported order.
//   kVADPCMErrLargePredictorCount: Predictor count is larger than the largest
//                                  supported predictor count.
//   kVADPCMErrUnknownVersion: Data uses an unknown version of VADPCM.
vadpcm_error vadpcm_read_codebook_aifc(
    struct vadpcm_codebook_spec *VADPCM_RESTRICT spec,
    size_t *VADPCM_RESTRICT data_offset, const void *VADPCM_RESTRICT data,
    size_t size);

// Parse codebook vectors.
void vadpcm_read_vectors(int count, const void *VADPCM_RESTRICT data,
                         struct vadpcm_vector *VADPCM_RESTRICT vectors);

// Decode VADPCM-encoded audio.
//
// Arguments:
//   predictor_count: Number of predictors in codebook
//   order: Predictor order in codebook
//   codebook: Array of predictor_count * order vectors in codebook
//   state: Decoder state, initially zero
//   frame_count: Number of frames of VADPCM to decode
//   dest: Output array of frame_count * kVADPCMFrameSampleCount elements
//   src: Input array of frame_count * kVADPCMFrameByteSize bytes
//
// Error codes:
//   kVADPCMErrInvalidData: Predictor index out of range.
vadpcm_error vadpcm_decode(int predictor_count, int order,
                           const struct vadpcm_vector *VADPCM_RESTRICT codebook,
                           struct vadpcm_vector *VADPCM_RESTRICT state,
                           size_t frame_count, int16_t *VADPCM_RESTRICT dest,
                           const void *VADPCM_RESTRICT src);

#ifdef __cplusplus
}
#endif
