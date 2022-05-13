// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "lib/vadpcm/test.h"

#include "lib/vadpcm/binary.h"
#include "lib/vadpcm/vadpcm.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>

#pragma GCC diagnostic ignored "-Wmultichar"

const char *vadpcm_error_name2(vadpcm_error err) {
    const char *msg = vadpcm_error_name(err);
    return msg == NULL ? "unknown error" : msg;
}

void *xmalloc(size_t nbytes) {
    if (nbytes == 0) {
        return NULL;
    }
    void *ptr = malloc(nbytes);
    if (ptr == NULL) {
        fputs("error: no memory\n", stderr);
        exit(1);
    }
    return ptr;
}

// Contents of a file.
struct filedata {
    void *data; // Must be freed with free().
    long size;
};

static void read_file_error(const char *path, const char *msg) {
    fprintf(stderr, "error: read_file %s: %s\n", path, msg);
}

static bool read_file(struct filedata *data, const char *path) {
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        read_file_error(path, strerror(errno));
        return false;
    }
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    if (len == 0) {
        read_file_error(path, "empty file");
        fclose(fp);
        return false;
    }
    fseek(fp, 0, SEEK_SET);
    uint8_t *ptr = malloc(len);
    if (ptr == NULL) {
        read_file_error(path, "no memory");
        fclose(fp);
        return false;
    }
    long pos = 0;
    while (pos < len) {
        size_t amt = fread(ptr + pos, 1, len - pos, fp);
        if (amt == 0) {
            if (feof(fp)) {
                read_file_error(path, "unexpected EOF");
            } else {
                read_file_error(path, strerror(errno));
            }
            free(ptr);
            fclose(fp);
            return false;
        }
        pos += amt;
    }
    fclose(fp);
    data->data = ptr;
    data->size = len;
    return true;
}

static const uint8_t kCodebookHeader[] = {
    's', 't', 'o', 'c', 11,  'V', 'A', 'D',
    'P', 'C', 'M', 'C', 'O', 'D', 'E', 'S',
};

// Contents of an AIFF file.
struct aiff {
    struct filedata data;

    const void *audio;
    uint32_t audio_size;

    const void *codebook;
    size_t codebook_size;
};

static void read_aiff_error(const char *path, const char *msg) {
    fprintf(stderr, "error: read_aiff %s: %s\n", path, msg);
}

static bool read_aiff(struct aiff *aiff, const char *path) {
    struct filedata data;
    if (!read_file(&data, path)) {
        return false;
    }
    if (data.size < 12) {
        read_aiff_error(path, "file too small");
        goto fail;
    }
    const uint8_t *ptr = data.data;
    uint32_t id = vadpcm_read32(ptr);
    uint32_t size = vadpcm_read32(ptr + 4);
    uint32_t form_type = vadpcm_read32(ptr + 8);
    if (id != 'FORM' || (form_type != 'AIFF' && form_type != 'AIFC')) {
        read_aiff_error(path, "not an AIFF or AIFC file");
        goto fail;
    }
    if (size > (uint32_t)data.size - 8) {
        read_aiff_error(path, "missing data");
        goto fail;
    }
    const uint8_t *end = ptr + 8 + size;
    ptr += 12;
    *aiff = (struct aiff){.data = data};
    while (end - ptr >= 8) {
        id = vadpcm_read32(ptr);
        size = vadpcm_read32(ptr + 4);
        ptr += 8;
        uint32_t advance = (size + 1) & ~(uint32_t)1;
        if (advance < size || size > (size_t)(end - ptr)) {
            read_aiff_error(path, "bad chunk");
            goto fail;
        }
        if (id == 'SSND') {
            if (size < 8) {
                read_aiff_error(path, "bad SSND chunk");
                goto fail;
            }
            aiff->audio = ptr + 8;
            aiff->audio_size = size - 8;
        } else if (id == 'APPL') {
            if (size >= sizeof(kCodebookHeader) &&
                memcmp(ptr, kCodebookHeader, sizeof(kCodebookHeader)) == 0) {
                aiff->codebook = ptr + sizeof(kCodebookHeader);
                aiff->codebook_size = size - sizeof(kCodebookHeader);
            }
        }
        ptr += advance;
    }
    if (aiff->audio_size == 0) {
        read_aiff_error(path, "no audio");
        goto fail;
    }
    return true;
fail:
    free(data.data);
    return false;
}

static void print_frame(const int16_t *ptr) {
    for (int i = 0; i < 16; i++) {
        fprintf(stderr, "%8d", ptr[i]);
    }
}

void show_pcm_diff(const int16_t *ref, const int16_t *out) {
    fputs("ref:", stderr);
    print_frame(ref);
    fputc('\n', stderr);

    fputs("out:", stderr);
    print_frame(out);
    fputc('\n', stderr);

    int pos = 0;
    for (int i = 0; i < 16; i++) {
        if (ref[i] != out[i]) {
            int col = 4 + 8 * i;
            while (pos < col) {
                fputc(' ', stderr);
                pos++;
            }
            col += 8;
            while (pos < col) {
                fputc('^', stderr);
                pos++;
            }
        }
    }
    fputc('\n', stderr);
}

const char *const kAIFFNames[] = {
    "sfx1",
    NULL,
};

int test_failure_count;

static void test_file(const char *name) {
    struct aiff aiff;
    int16_t *pcm = NULL;
    char path[128];
    aiff.data.data = NULL;

    // Read PCM file.
    snprintf(path, sizeof(path), "lib/vadpcm/data/%s.pcm.aiff", name);
    if (!read_aiff(&aiff, path)) {
        test_failure_count++;
        return;
    }
    size_t sample_count = aiff.audio_size / 2;
    pcm = xmalloc(sizeof(*pcm) * sample_count);
    for (size_t i = 0; i < sample_count; i++) {
        pcm[i] = vadpcm_read16((const char *)aiff.audio + 2 * i);
    }
    free(aiff.data.data);
    aiff.data.data = NULL;

    // Read VADPCM file.
    snprintf(path, sizeof(path), "lib/vadpcm/data/%s.adpcm.aifc", name);
    if (!read_aiff(&aiff, path)) {
        test_failure_count++;
        goto done;
    }
    size_t frame_count = aiff.audio_size / kVADPCMFrameByteSize;
    if (sample_count != frame_count * kVADPCMFrameSampleCount) {
        fprintf(stderr,
                "error: %s: mismatched sample count: "
                "ADPCM = %zu samples, PCM = %zu samples\n",
                name, frame_count * kVADPCMFrameSampleCount, sample_count);
        test_failure_count++;
        goto done;
    }
    size_t cboffset;
    struct vadpcm_codebook_spec cbspec;
    vadpcm_error err = vadpcm_read_codebook_aifc(
        &cbspec, &cboffset, aiff.codebook, aiff.codebook_size);
    if (err != 0) {
        fprintf(stderr, "error: read_codebook %s: %s\n", path,
                vadpcm_error_name2(err));
    }
    if (cbspec.order != kVADPCMEncodeOrder) {
        fprintf(stderr, "error: %s: order is not 2: order = %d\n", path,
                cbspec.order);
        test_failure_count++;
        goto done;
    }
    struct vadpcm_vector cbvec[kVADPCMEncodeOrder * kVADPCMMaxPredictorCount];
    vadpcm_read_vectors(cbspec.order * cbspec.predictor_count,
                        (const char *)aiff.codebook + cboffset, cbvec);
    const void *vadpcm = aiff.audio;

    // Run tests.
    test_decode(name, cbspec.predictor_count, cbspec.order, cbvec, frame_count,
                vadpcm, pcm);
    test_reencode(name, cbspec.predictor_count, cbspec.order, cbvec,
                  frame_count, vadpcm);

done:
    free(aiff.data.data);
    free(pcm);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    test_encoder();
    for (int i = 0; kAIFFNames[i] != NULL; i++) {
        test_file(kAIFFNames[i]);
    }

    if (test_failure_count > 0) {
        fprintf(stderr, "tests failed: %d\n", test_failure_count);
        return 1;
    }
    fputs("all tests passed\n", stderr);
    return 0;
}
