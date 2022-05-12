// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#pragma once

#include <string_view>

namespace util {

// Return the basename of a path.
std::string_view Basename(std::string_view path);

// Return the file extension for a path.
std::string_view Extension(std::string_view path);

} // namespace util
