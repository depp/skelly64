// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "lib/vadpcm/binary.h"
#include "lib/vadpcm/test.h"
#include "lib/vadpcm/vadpcm.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static bool did_fail;

static void print_verr(const char *name, const char *func, vadpcm_error err) {
    const char *msg = vadpcm_error_name(err);
    if (msg == NULL) {
        msg = "unknown error";
    }
    fprintf(stderr, "error: %s %s: %s\n", name, func, msg);
}

static void print_frame(const char *name, const int16_t *ptr) {
    fprintf(stderr, "%s:", name);
    for (int i = 0; i < kVADPCMFrameSampleCount; i++) {
        fprintf(stderr, "%8d", ptr[i]);
    }
    fputc('\n', stderr);
}

static void test_decode(const char *name) {
    char path[256];
    struct aiff adpcm, pcm;
    int16_t *ref_pcm = NULL, *out_pcm = NULL;
    struct vadpcm_codebook *b = NULL;
    snprintf(path, sizeof(path), "lib/vadpcm/data/%s.adpcm.aifc", name);
    read_aiff(&adpcm, path);
    snprintf(path, sizeof(path), "lib/vadpcm/data/%s.pcm.aiff", name);
    read_aiff(&pcm, path);

    size_t frame_count = adpcm.audio_size / kVADPCMFrameByteSize;
    size_t sample_count = pcm.audio_size / 2;
    if (frame_count * kVADPCMFrameSampleCount != sample_count) {
        fprintf(stderr,
                "error: %s: mismatched sample count: "
                "ADPCM = %zu samples, PCM = %zu samples\n",
                name, frame_count * kVADPCMFrameSampleCount, sample_count);
        did_fail = true;
        goto done;
    }
    ref_pcm = xmalloc(sample_count * 2);
    for (size_t i = 0; i < sample_count; i++) {
        ref_pcm[i] = vadpcm_read16((const uint8_t *)pcm.audio + 2 * i);
    }
    vadpcm_error err =
        vadpcm_read_codebook_aifc(&b, adpcm.codebook, adpcm.codebook_size);
    if (err != 0) {
        print_verr(name, "read_codebook", err);
        did_fail = true;
        goto done;
    }
    out_pcm = xmalloc(sample_count * 2);
    struct vadpcm_state state = {{0}};
    err = vadpcm_decode(b, &state, frame_count, out_pcm, adpcm.audio);
    if (err != 0) {
        print_verr(name, "decode", err);
        did_fail = true;
        goto done;
    }
    for (size_t i = 0; i < sample_count; i++) {
        if (ref_pcm[i] != out_pcm[i]) {
            fprintf(stderr,
                    "error: decoder output does not match, "
                    "index = %zu\n",
                    i);
            size_t frame = i / kVADPCMFrameSampleCount;
            print_frame("ref", ref_pcm + frame * kVADPCMFrameSampleCount);
            print_frame("out", out_pcm + frame * kVADPCMFrameSampleCount);
            did_fail = true;
            goto done;
        }
    }

done:
    free(adpcm.data.data);
    free(pcm.data.data);
    free(b);
    free(ref_pcm);
    free(out_pcm);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    test_decode("sfx1");
    return did_fail ? 1 : 0;
}
