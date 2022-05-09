// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#pragma once

#include <string>
#include <string_view>

namespace util {

// Quote a string with double quotes and escaepe the contents.
std::string Quote(std::string_view str);

} // namespace util
