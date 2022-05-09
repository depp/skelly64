// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#pragma once

#include "lib/cpp/expr.hpp"
#include "lib/cpp/flag.hpp"

namespace util {

// A flag that contains as expression.
class ExprFlag : public flag::FlagBase {
    Expr::Ref *m_ptr;

public:
    explicit ExprFlag(Expr::Ref *ptr) : m_ptr{ptr} {}
    flag::FlagArgument Argument() const override;
    void Parse(std::optional<std::string_view> arg) override;
};

} // namespace util
