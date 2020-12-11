#include "base/tool.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

int xatoi(const char *s) {
    if (*s == '\0') {
        die("empty string is not a valid integer");
    }
    char *end;
    errno = 0;
    long v = strtol(s, &end, 0);
    if (*end != '\0') {
        die("invalid integer");
    }
    if (v < INT_MIN || v > INT_MAX ||
        ((v == LONG_MIN || v == LONG_MAX) && errno == ERANGE)) {
        die("number out of range");
    }
    return v;
}
