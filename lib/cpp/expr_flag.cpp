// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "lib/cpp/expr_flag.hpp"

#include <cassert>
#include <fmt/core.h>

namespace util {

flag::FlagArgument ExprFlag::Argument() const {
    return flag::FlagArgument::Required;
}
void ExprFlag::Parse(std::optional<std::string_view> arg) {
    assert(arg.has_value());
    try {
        *m_ptr = Expr::Parse(*arg);
    } catch (ExprParseError &ex) {
        std::string msg = fmt::format("invalid expression: {}", ex.what());
        throw flag::UsageError(msg);
    }
}

} // namespace util
