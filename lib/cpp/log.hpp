// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#pragma once

#include <fmt/format.h>

namespace util {

// Logging severity levels.
enum class LogLevel {
    Error,
    Warning,
    Info,
    Debug,
};

// Log a message to the console.
void LogV(LogLevel level, fmt::string_view format, fmt::format_args args);

// Log a message to the console.
template <typename S, typename... Args>
void Log(LogLevel level, const S &format, Args &&...args) {
    LogV(level, format, fmt::make_args_checked<Args...>(format, args...));
}

// Print an error message.
void ErrV(fmt::string_view format, fmt::format_args args);

// Print an error message.
template <typename S, typename... Args>
void Err(const S &format, Args &&...args) {
    ErrV(format, fmt::make_args_checked<Args...>(format, args...));
}

// Print a warning message.
void WarnV(fmt::string_view format, fmt::format_args args);

// Print a warning message.
template <typename S, typename... Args>
void Warn(const S &format, Args &&...args) {
    WarnV(format, fmt::make_args_checked<Args...>(format, args...));
}

} // namespace util
