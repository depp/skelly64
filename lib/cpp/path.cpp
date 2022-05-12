// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "lib/cpp/path.hpp"

namespace util {

std::string_view Basename(std::string_view path) {
    size_t i = path.rfind('/');
    return i == std::string_view::npos ? path : path.substr(i + 1);
}

std::string_view Extension(std::string_view path) {
    std::string_view base = Basename(path);
    size_t i = base.rfind('.');
    if (i == 0 || i == std::string_view::npos) {
        return std::string_view();
    }
    return base.substr(i);
}

} // namespace util
