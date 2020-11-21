#include "tools/util/expr.hpp"

#include <cstdlib>
#include <cstring>
#include <fmt/core.h>

using namespace util;

namespace {

bool RunExpression(Expr::Env &env, const char *arg) {
    const char *eq = std::strchr(arg, '=');
    std::string ident;
    if (eq != nullptr) {
        try {
            ident = Expr::ParseIdent(std::string_view(arg, eq - arg));
        } catch (ExprParseError &ex) {
            fmt::print(stderr, "Error: bad assignment: {}\n", ex.what());
            return false;
        }
        arg = eq + 1;
    }
    Expr::Ref expr;
    try {
        expr = Expr::Parse(std::string_view{arg});
    } catch (ExprParseError &ex) {
        fmt::print(stderr, "Error: bad expression: {}\n", ex.what());
        return false;
    }
    fmt::print("Expression: {}\n", expr->ToString());
    double value;
    try {
        value = expr->Eval(env);
    } catch (ExprEvalError &ex) {
        fmt::print(stderr, "Error: could not evaluate: {}\n", ex.what());
        return false;
    }
    fmt::print("Result: {}\n", value);
    if (!ident.empty()) {
        env.insert_or_assign(std::move(ident), value);
    }
    return true;
}

} // namespace

int main(int argc, char **argv) {
    using namespace util;
    Expr::Env env;
    bool ok = true;
    for (int i = 1; i < argc; i++) {
        if (!RunExpression(env, argv[i])) {
            ok = false;
        }
    }
    return ok ? 0 : 1;
}
