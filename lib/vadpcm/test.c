// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "lib/vadpcm/test.h"

#include "lib/vadpcm/binary.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>

#pragma GCC diagnostic ignored "-Wmultichar"

noreturn static void read_fail(const char *path, const char *msg) {
    fprintf(stderr, "error: read %s: %s\n", path, msg);
    exit(1);
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

void read_file(struct filedata *data, const char *path) {
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

static const uint8_t kCodebookHeader[] = {
    's', 't', 'o', 'c', 11,  'V', 'A', 'D',
    'P', 'C', 'M', 'C', 'O', 'D', 'E', 'S',
};

void read_aiff(struct aiff *aiff, const char *path) {
    struct filedata data;
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

const char *const kAIFFNames[] = {
    "sfx1",
    NULL,
};

void read_aiff_pcm(struct aiff *aiff, const char *name) {
    char path[128];
    snprintf(path, sizeof(path), "lib/vadpcm/data/%s.pcm.aiff", name);
    read_aiff(aiff, path);
}

void read_aiff_vadpcm(struct aiff *aiff, const char *name) {
    char path[128];
    snprintf(path, sizeof(path), "lib/vadpcm/data/%s.adpcm.aifc", name);
    read_aiff(aiff, path);
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
