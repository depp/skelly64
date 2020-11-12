#include "base/tool.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

noreturn void die_(const char *file, int line, const char *fmt, ...) {
    fprintf(stderr, "Error: %s:%d: ", file, line);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

noreturn void die_errno_(const char *file, int line, int err, const char *fmt,
                         ...) {
    fprintf(stderr, "Error: %s:%d: ", file, line);
    va_list(ap);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputs(": ", stderr);
    fputs(strerror(err), stderr);
    fputc('\n', stderr);
    exit(1);
}