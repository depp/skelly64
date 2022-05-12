// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "lib/cpp/log.hpp"

#include <string_view>

namespace util {

void LogV(LogLevel level, fmt::string_view format, fmt::format_args args) {
    std::string_view color, leveltext;
    switch (level) {
    case LogLevel::Error:
        color = "31";
        leveltext = "Error";
        break;
    case LogLevel::Warning:
        color = "33";
        leveltext = "Warning";
        break;
    case LogLevel::Info:
        leveltext = "Info";
        break;
    case LogLevel::Debug:
        color = "36";
        leveltext = "Debug";
        break;
    }
    auto buf = fmt::memory_buffer();
    auto out = std::back_inserter(buf);
    if (!color.empty()) {
        fmt::format_to(out, "\x1b[{}m{}\x1b[0m: ", color, leveltext);
    } else {
        fmt::format_to(out, "{}: ", leveltext);
    }
    fmt::vformat_to(out, format, args);
    buf.push_back('\n');
    fwrite(buf.data(), 1, buf.size(), stderr);
}

void ErrV(fmt::string_view format, fmt::format_args args) {
    LogV(LogLevel::Error, format, args);
}

void WarnV(fmt::string_view format, fmt::format_args args) {
    LogV(LogLevel::Warning, format, args);
}

void InfoV(fmt::string_view format, fmt::format_args args) {
    LogV(LogLevel::Info, format, args);
}

void DebugV(fmt::string_view format, fmt::format_args args) {
    LogV(LogLevel::Debug, format, args);
}

} // namespace util
