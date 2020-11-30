#include "tools/modelconvert/axes.hpp"

#include <fmt/core.h>

namespace modelconvert {

std::string Axes::ToString() const {
    std::string r;
    for (int i = 0; i < 3; i++) {
        if (i != 0) {
            r.push_back(',');
        }
        if (negate[i]) {
            r.push_back('-');
        }
        const int n = index[i];
        if (n < 0 || n >= 3) {
            throw std::logic_error("invalid Axes");
        }
        r.push_back("XYZ"[n]);
    }
    return r;
}

namespace {

bool IsWhite(char c) {
    return c == ' ' || c == '\t';
}

const char *SkipWhite(const char *ptr, const char *end) {
    while (ptr != end && IsWhite(*ptr)) {
        ptr++;
    }
    return ptr;
}

} // namespace

Axes Axes::Parse(std::string_view s) {
    const char *ptr = s.begin(), *end = s.end();
    Axes r;
    for (int i = 0; i < 3; i++) {
        ptr = SkipWhite(ptr, end);
        if (i != 0) {
            if (ptr != end && (*ptr == ':' || *ptr == ',')) {
                ptr++;
            }
        }
        ptr = SkipWhite(ptr, end);
        if (ptr == end) {
            throw std::invalid_argument("not enough components");
        }
        bool negate = false;
        if (ptr != end && *ptr == '-') {
            negate = true;
            ptr++;
        }
        r.negate[i] = negate;
        ptr = SkipWhite(ptr, end);
        if (ptr == end) {
            throw std::invalid_argument("bad axis");
        }
        int index;
        switch (*ptr) {
        case 'x':
        case 'X':
            index = 0;
            break;
        case 'y':
        case 'Y':
            index = 1;
            break;
        case 'z':
        case 'Z':
            index = 2;
            break;
        default:
            throw std::invalid_argument("bad axis");
        }
        ptr++;
        r.index[i] = index;
        for (int j = 0; j < i; j++) {
            if (r.index[j] == index) {
                std::string msg =
                    fmt::format("duplicate axis: {}", "XYZ"[index]);
                throw std::invalid_argument(msg);
            }
        }
    }
    ptr = SkipWhite(ptr, end);
    if (ptr != end) {
        throw std::invalid_argument("extra data after axes");
    }
    return r;
}

} // namespace modelconvert
