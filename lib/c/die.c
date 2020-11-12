#include "base/tool.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

noreturn void die(const char *fmt, ...) {
    fputs("Error: ", stderr);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

noreturn void die_errno(int err, const char *fmt, ...) {
    fputs("Error: ", stderr);
    va_list(ap);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputs(": ", stderr);
    fputs(strerror(err), stderr);
    fputc('\n', stderr);
    exit(1);
}