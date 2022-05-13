// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#pragma once

#include "lib/vadpcm/vadpcm.h"

#include <stddef.h>
#include <stdint.h>

struct vadpcm_vector;

extern int test_failure_count;

// Return the name of the error, or "unknown error".
const char *vadpcm_error_name2(vadpcm_error err);

// Allocate memory, or abort on failure.
void *xmalloc(size_t nbytes);

// Print a frame of audio data, compared against a reference. The reference data
// comes first.
void show_pcm_diff(const int16_t *ref, const int16_t *out);

// Test that decoding the VADPCM results in an exact match for the known output.
void test_decode(const char *name, int predictor_count, int order,
                 struct vadpcm_vector *codebook, size_t frame_count,
                 const void *vadpcm, const int16_t *pcm);

// Test that re-encoding the VADPCM doesn't change the decoded audio.
void test_reencode(const char *name, int predictor_count, int order,
                   struct vadpcm_vector *codebook, size_t frame_count,
                   const void *vadpcm);

// Internal encoder tests.
void test_encoder(void);
