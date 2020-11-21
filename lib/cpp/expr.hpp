#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace util {

class ExprParseError : public std::runtime_error {
public:
    ExprParseError(const std::string &what) : runtime_error{what} {}
    ExprParseError(const char *what) : runtime_error{what} {}
};

class ExprEvalError : public std::runtime_error {
public:
    ExprEvalError(const std::string &what) : runtime_error{what} {}
    ExprEvalError(const char *what) : runtime_error{what} {}
};

// An expression.
struct Expr {
    using Env = std::unordered_map<std::string, double>;
    using Ref = std::unique_ptr<Expr>;
    virtual ~Expr() = default;
    virtual double Eval(const Env &env) const = 0;
    virtual void Append(std::string *out, int prec) const = 0;
    std::string ToString() const;
    static Ref Parse(std::string_view text);
    static std::string ParseIdent(std::string_view text);
};

// A literal value.
struct Literal final : Expr {
    double value;
    explicit Literal(double value) : value{value} {}
    double Eval(const Env &env) const override;
    void Append(std::string *out, int prec) const override;
};

// A reference to a variable.
struct VarRef final : Expr {
    std::string name;
    explicit VarRef(std::string name) : name{std::move(name)} {}
    double Eval(const Env &env) const override;
    void Append(std::string *out, int prec) const override;
};

// Unary operator.
enum class UnOp {
    Neg,
};

// Unary operator expression.
struct UnExpr final : Expr {
    UnOp op;
    Ref arg;
    explicit UnExpr(UnOp op, Ref arg) : op{op}, arg{std::move(arg)} {}
    double Eval(const Env &env) const override;
    void Append(std::string *out, int prec) const override;
};

// Binary operator.
enum class BinOp {
    Add,
    Sub,
    Mul,
    Div,
};

// Binary operator expression.
struct BinExpr final : Expr {
    BinOp op;
    Ref args[2];
    explicit BinExpr(BinOp op, Ref lhs, Ref rhs)
        : op{op}, args{std::move(lhs), std::move(rhs)} {}
    double Eval(const Env &env) const override;
    void Append(std::string *out, int prec) const override;
};

} // namespace util
