#pragma once

#include "tools/util/expr.hpp"
#include "tools/util/flag.hpp"

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
