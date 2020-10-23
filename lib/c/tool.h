// Library functions for writing tools.
#pragma once
#include "base/defs.h"

#include <stdnoreturn.h>

// Print an error message and exit.
noreturn void die(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// Print an error message with an errno error message and exit.
noreturn void die_errno(int err, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
