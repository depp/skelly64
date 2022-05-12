// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#pragma once

// Library functions for writing tools.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdnoreturn.h>

// ARRAY_COUNT evaluates to the compile-time size of the array, which must be an
// array and not a pointer to an element.
#define ARRAY_COUNT(x) (sizeof(x) / sizeof(*(x)))

// Print an error message and exit.
noreturn void die_(const char *file, int line, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

#define die(...) die_(__FILE__, __LINE__, __VA_ARGS__)

// Print an error message with an errno error message and exit.
noreturn void die_errno_(const char *file, int line, int err, const char *fmt,
                         ...) __attribute__((format(printf, 4, 5)));

#define die_errno(err, ...) die_errno_(__FILE__, __LINE__, err, __VA_ARGS__)

// Print an error message for a failed read. This will either be an IO error or
// an unexpected EOF, depending on the state of ferror(fp) and feof(fp).
noreturn void die_read_(const char *file, int line, FILE *fp, const char *fmt,
                        ...) __attribute__((format(printf, 4, 5)));
#define die_read(fp, ...) die_read_(__FILE__, __LINE__, fp, __VA_ARGS__)

// Convert to integer, abort on failure.
int xatoi(const char *s);
