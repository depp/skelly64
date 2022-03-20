#include "lib/vadpcm/codebook.h"
#include "lib/vadpcm/vadpcm.h"

#include <limits.h>

#include <emmintrin.h>

#define SHUFFLE(i) _MM_SHUFFLE(i, i, i, i)

#define ACCUM_FULL(p, i)                                          \
    sample = _mm_shuffle_epi32(samp4, SHUFFLE((i)&3));            \
    mullo = _mm_mullo_epi16(sample, p);                           \
    mulhi = _mm_mulhi_epi16(sample, p);                           \
    acc0 = _mm_add_epi32(acc0, _mm_unpacklo_epi16(mullo, mulhi)); \
    acc1 = _mm_add_epi32(acc1, _mm_unpackhi_epi16(mullo, mulhi))

#define ACCUM_8(i)                      \
    p = _mm_bslli_si128(p1, 2 + (i)*2); \
    ACCUM_FULL(p, i)

vadpcm_error vadpcm_decode_sse2(const struct vadpcm_codebook *restrict b,
                                struct vadpcm_state *restrict state,
                                size_t framecount, int16_t *restrict dest,
                                const void *restrict src) {
    const uint8_t *sptr = src;
    __m128i prev = _mm_loadu_si128((void *)state->v);
    for (size_t frame = 0; frame < framecount; frame++) {
        const uint8_t *fin = sptr + kVADPCMFrameByteSize * frame;

        // Control byte: scaling & predictor index.
        int control = fin[0];
        // if (control >> 4) is 0, then we want to shift down all the way back
        // to the ones place, 12 places.
        __m128i shift = _mm_cvtsi32_si128(12 - (control >> 4));
        int predictor_index = control & 15;
        if (predictor_index >= b->predictor_count) {
            return kVADPCMErrInvalidData;
        }
        const struct vadpcm_vector *restrict predictor =
            vadpcm_codebook_get(b, predictor_index);
        __m128i p0 = _mm_load_si128((const void *)predictor[0].v);
        __m128i p1 = _mm_load_si128((const void *)predictor[1].v);

        // Load each byte into the high byte of an epi16.
        __m128i encoded = _mm_unpacklo_epi8(
            _mm_setzero_si128(), _mm_loadl_epi64((const void *)(fin + 1)));

        // Decode each of the two vectors within the frame.
        for (int vector = 0; vector < 2; vector++) {
            __m128i samp8, samp4, sample, mulhi, mullo, p, acc0, acc1;

            // Get the 8 samples we care about.
            samp8 = _mm_sra_epi16(
                _mm_unpacklo_epi16(
                    _mm_and_si128(encoded, _mm_set1_epi16(0xf000)),
                    _mm_slli_epi16(encoded, 4)),
                shift);

            // Initialize the accumulator with the residuals, scaled to unity (1
            // << 11).
            acc0 = _mm_srai_epi32(
                _mm_unpacklo_epi16(_mm_setzero_si128(), samp8), 5);
            acc1 = _mm_srai_epi32(
                _mm_unpackhi_epi16(_mm_setzero_si128(), samp8), 5);

            // Accumulate the predictor from the last 2 samples.
            samp4 = _mm_unpackhi_epi16(prev, prev);
            ACCUM_FULL(p0, 2);
            ACCUM_FULL(p1, 3);

            // Process the first four samples.
            samp4 = _mm_unpacklo_epi16(samp8, samp8);
            ACCUM_8(0);
            ACCUM_8(1);
            ACCUM_8(2);
            ACCUM_8(3);

            // Process the next four samples.
            samp4 = _mm_unpackhi_epi16(samp8, samp8);
            ACCUM_8(4);
            ACCUM_8(5);
            ACCUM_8(6);

            // Store the result.
            prev = _mm_packs_epi32(_mm_srai_epi32(acc0, 11),
                                   _mm_srai_epi32(acc1, 11));
            _mm_storeu_si128(
                (void *)(dest + kVADPCMFrameSampleCount * frame + 8 * vector),
                prev);

            // Next time, get the next 8 samples.
            encoded = _mm_unpackhi_epi64(encoded, encoded);
        }
    }
    _mm_storeu_si128((void *)state->v, prev);
    return 0;
}
