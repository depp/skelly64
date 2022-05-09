// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#pragma once

#include <stdexcept>
#include <string>

namespace util {

// Generic subclass for fatal errors. This should only be used for runtime
// errors that terminate the program, and carry a descriptive, human-readable
// error message.
class Error : public std::runtime_error {
public:
    using runtime_error::runtime_error;
};

// Return the description of the given error code.
std::string StrError(int errorcode);

// Construct an I/O error.
Error IOError(std::string_view file, const char *op, int errorcode);

// Construct an error for an unexpected end of file.
Error UnexpectedEOF(std::string_view file);

} // namespace util
