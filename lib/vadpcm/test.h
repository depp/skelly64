// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#pragma once

#include <stddef.h>
#include <stdint.h>

// Contents of a file.
struct filedata {
    void *data; // Must be freed with free().
    long size;
};

// Contents of an AIFF file.
struct aiff {
    struct filedata data;
    const void *audio;
    uint32_t audio_size;
    const void *codebook;
    uint32_t codebook_size;
};

// Allocate memory, or abort on failure.
void *xmalloc(size_t nbytes);

// Read an entire file into memory. Abort on failure.
void read_file(struct filedata *data, const char *path);

// Read an AIFF or AIFC file into memory and extract the pointers to the
// codebook (if present) and audio data. Anything else in the file is ignored.
void read_aiff(struct aiff *aiff, const char *path);

// NULL-terminated base names of AIFF files with test data.
extern const char *const kAIFFNames[];

// Read the AIFF file with PCM data for the given base name.
void read_aiff_pcm(struct aiff *aiff, const char *name);

// Read the AIFF file with VADPCM data for the given base name.
void read_aiff_vadpcm(struct aiff *aiff, const char *name);

// Print a frame of audio data, compared against a reference. The reference data
// comes first.
void show_pcm_diff(const int16_t *ref, const int16_t *out);
