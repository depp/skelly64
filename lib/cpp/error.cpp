#include "lib/cpp/error.hpp"

#include <fmt/format.h>

#include <string.h>

namespace util {

namespace {

static constexpr std::size_t kErrorBufSize = 1024;

}

#if _POSIX_C_SOURCE >= 200112L && !_GNU_SOURCE
std::string StrError(int errorcode) {
    char buf[kErrorBufSize];
    int r = strerror_r(errorcode, buf, sizeof(buf));
    if (r != 0) {
        return std::string();
    }
    return buf;
}
#else
std::string StrError(int errorcode) {
    char buf[kErrorBufSize];
    char *p = strerror_r(errorcode, buf, sizeof(buf));
    if (p == nullptr) {
        return std::string();
    }
    return p;
}
#endif

Error IOError(std::string_view file, const char *op, int errorcode) {
    return Error(fmt::format("{} {}: {}", op, file, util::StrError(errorcode)));
}

Error UnexpectedEOF(std::string_view file) {
    return Error(fmt::format("read {}: unexpected end of file", file));
}

} // namespace util
