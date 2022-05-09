#pragma once
// VADPCM encoding and decoding.

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

    // Memory allocation failed.
    kVADPCMErrNoMemory,

    // Invalid data.
    kVADPCMErrInvalidData,

    // Predictor order is too large.
    kVADPCMErrLargeOrder,

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
};

struct vadpcm_codebook;

// The state of a VADPCM decoder.
struct vadpcm_state {
    int16_t v[kVADPCMMaxOrder];
};

// Read a codebook, as it appears in an AIFC file. On success, stores a pointer
// to a new codebook in b. On failure, returns an error code.
//
// The data is taken from an AIFC 'APPL' (application-specific) chunk with the
// name "VADPCMCODES". The chunk header, APPL header, and chunk name should not
// be included in the data passed to this function.
//
// Error codes:
//   kVADPCMErrNoMemory: Failed to allocate codebook.
//   kVADPCMErrInvalidData: Order or predictor count is zero, or the data is
//                          incomplete (unexpected EOF).
//   kVADPCMErrLargeOrder: Order is larger than largest supported order.
vadpcm_error vadpcm_read_codebook_aifc(struct vadpcm_codebook **b,
                                       const void *data, size_t size);

// Decode VADPCM-encoded audio. The output array have have
// kVADPCMFrameSampleCount * frame_count elements. The input data should have
// kVADPCMFrameByteSize * frame_count bytes.
//
// Error codes:
//   kVADPCMErrInvalidData: Predictor index out of range.
vadpcm_error vadpcm_decode(const struct vadpcm_codebook *VADPCM_RESTRICT b,
                           struct vadpcm_state *VADPCM_RESTRICT state,
                           size_t frame_count, int16_t *VADPCM_RESTRICT dest,
                           const void *VADPCM_RESTRICT src);

#ifdef __cplusplus
}
#endif
