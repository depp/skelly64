// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "lib/vadpcm/binary.h"
#include "lib/vadpcm/vadpcm.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>

#pragma GCC diagnostic ignored "-Wmultichar"

noreturn static void read_fail(const char *path, const char *msg) {
    fprintf(stderr, "error: read %s: %s\n", path, msg);
    exit(1);
}

static void *xmalloc(size_t nbytes) {
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

struct data {
    void *data;
    long size;
};

// Read an entire file into memory.
static void read_file(struct data *data, const char *path) {
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "error: open %s: %s\n", path, strerror(errno));
        exit(1);
    }
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    if (len == 0) {
        read_fail(path, "empty");
    }
    fseek(fp, 0, SEEK_SET);
    uint8_t *ptr = malloc(len);
    if (ptr == NULL) {
        read_fail(path, "no memory");
    }
    long pos = 0;
    while (pos < len) {
        size_t amt = fread(ptr + pos, 1, len - pos, fp);
        if (amt == 0) {
            if (feof(fp)) {
                read_fail(path, "unexpected EOF");
            }
            read_fail(path, strerror(errno));
        }
        pos += amt;
    }
    fclose(fp);
    data->data = ptr;
    data->size = len;
}

struct aiff {
    struct data data;
    const void *audio;
    uint32_t audio_size;
    const void *codebook;
    uint32_t codebook_size;
};

static const uint8_t kCodebookHeader[] = {
    's', 't', 'o', 'c', 11,  'V', 'A', 'D',
    'P', 'C', 'M', 'C', 'O', 'D', 'E', 'S',
};

// Read an AIFF file into memory and parse the chunks.
static void read_aiff(struct aiff *aiff, const char *path) {
    struct data data;
    read_file(&data, path);
    if (data.size < 12) {
        read_fail(path, "too small");
    }
    const uint8_t *ptr = data.data;
    uint32_t id = vadpcm_read32(ptr);
    uint32_t size = vadpcm_read32(ptr + 4);
    uint32_t form_type = vadpcm_read32(ptr + 8);
    if (id != 'FORM' || (form_type != 'AIFF' && form_type != 'AIFC')) {
        read_fail(path, "not an AIFF or AIFC file");
    }
    if (size > (uint32_t)data.size - 8) {
        read_fail(path, "incorrect form size");
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
            read_fail(path, "bad chunk");
        }
        if (id == 'SSND') {
            if (size < 8) {
                read_fail(path, "bad SSND chunk");
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
        read_fail(path, "no audio");
    }
}

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
