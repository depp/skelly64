#include "tools/util/expr.hpp"

#include <cmath>
#include <fmt/core.h>
#include <system_error>

namespace util {

namespace {

bool IsSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool IsIdent(char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') ||
           ('0' <= c && c <= '9') || c == '_';
}

bool IsDigit(char c) {
    return '0' <= c && c <= '9';
}

enum class Token {
    End,
    Number,
    Ident,
    Add,
    Sub,
    Mul,
    Div,
    OpenParen,
    CloseParen,
};

const char *TokenName(Token tok) {
    switch (tok) {
    case Token::End:
        return "end";
    case Token::Number:
        return "number";
    case Token::Ident:
        return "identifier";
    case Token::Add:
        return "+";
    case Token::Sub:
        return "-";
    case Token::Mul:
        return "*";
    case Token::Div:
        return "/";
    case Token::OpenParen:
        return "(";
    case Token::CloseParen:
        return ")";
    }
    return "<unknown>";
}

class Tokenizer {
    const char *m_ptr, *m_end;
    Token m_tok;
    std::string_view m_toktext;

public:
    explicit Tokenizer(std::string_view data)
        : m_ptr{data.begin()}, m_end{data.end()} {}

    Token tok() const { return m_tok; }
    std::string_view text() const { return m_toktext; }

    void Next() {
        while (m_ptr != m_end && IsSpace(*m_ptr)) {
            m_ptr++;
        }
        if (m_ptr == m_end) {
            m_tok = Token::End;
            return;
        }
        const char *start = m_ptr++;
        char c = *start;
        Token tok;
        switch (c) {
        case '+':
            tok = Token::Add;
            break;
        case '-':
            tok = Token::Sub;
            break;
        case '*':
            tok = Token::Mul;
            break;
        case '/':
            tok = Token::Div;
            break;
        case '(':
            tok = Token::OpenParen;
            break;
        case ')':
            tok = Token::CloseParen;
            break;
        default:
            if (('0' <= c && c <= '9') || c == '.') {
                tok = Token::Number;
                while (m_ptr != m_end && (IsDigit(*m_ptr) || *m_ptr == '.')) {
                    m_ptr++;
                }
                if (m_end - m_ptr >= 2 && (*m_ptr == 'e' || *m_ptr == 'E')) {
                    c = *(m_ptr + 1);
                    if (IsDigit(c) || c == '-' || c == '+') {
                        m_ptr += 2;
                        while (m_ptr != m_end && IsDigit(*m_ptr)) {
                            m_ptr++;
                        }
                    }
                }
            } else if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') ||
                       c == '_') {
                tok = Token::Ident;
                while (m_ptr != m_end && IsIdent(*m_ptr)) {
                    m_ptr++;
                }
            } else {
                std::string chr;
                if (32 <= c && c <= 126) {
                    if (c == '\\' || c == '\'') {
                        chr = fmt::format("<{}>", c);
                    } else {
                        chr = fmt::format("'{}'", c);
                    }
                }
                std::string msg = fmt::format("unexpected character: {}", chr);
                throw ExprParseError(msg);
            }
        }
        m_tok = tok;
        m_toktext = std::string_view(start, m_ptr - start);
    }
};

Expr::Ref ParseAdd(Tokenizer &toks);
Expr::Ref ParseMul(Tokenizer &toks);
Expr::Ref ParseAtom(Tokenizer &toks);

Expr::Ref ParseAdd(Tokenizer &toks) {
    Expr::Ref expr = ParseMul(toks);
    for (;;) {
        BinOp op;
        switch (toks.tok()) {
        case Token::Add:
            op = BinOp::Add;
            break;
        case Token::Sub:
            op = BinOp::Sub;
            break;
        default:
            return expr;
        }
        toks.Next();
        expr = std::make_unique<BinExpr>(op, std::move(expr), ParseMul(toks));
    }
}

Expr::Ref ParseMul(Tokenizer &toks) {
    Expr::Ref expr = ParseAtom(toks);
    for (;;) {
        BinOp op;
        switch (toks.tok()) {
        case Token::Mul:
            op = BinOp::Mul;
            break;
        case Token::Div:
            op = BinOp::Div;
            break;
        default:
            return expr;
        }
        toks.Next();
        expr = std::make_unique<BinExpr>(op, std::move(expr), ParseAtom(toks));
    }
}

Expr::Ref ParseAtom(Tokenizer &toks) {
    switch (toks.tok()) {
    case Token::Add:
        toks.Next();
        return ParseAtom(toks);
    case Token::Sub:
        toks.Next();
        return std::make_unique<UnExpr>(UnOp::Neg, ParseAtom(toks));
    case Token::OpenParen: {
        toks.Next();
        Expr::Ref expr = ParseAdd(toks);
        if (toks.tok() != Token::CloseParen) {
            throw ExprParseError("missing close ')'");
        }
        toks.Next();
        return expr;
    }
    case Token::Number: {
        std::string text{toks.text()};
        toks.Next();
        double value;
        size_t pos;
        try {
            value = std::stod(text, &pos);
        } catch (std::invalid_argument &ex) {
            std::string msg = fmt::format("invalid number: '{}'", text);
            throw ExprParseError(msg);
        } catch (std::out_of_range &ex) {
            std::string msg = fmt::format("number out of range: '{}'", text);
            throw ExprParseError(msg);
        }
        if (pos != text.size()) {
            std::string msg = fmt::format("invalid number: '{}'", text);
            throw ExprParseError(msg);
        }
        return std::make_unique<Literal>(value);
    }
    case Token::Ident: {
        std::string_view text = toks.text();
        toks.Next();
        return std::make_unique<VarRef>(std::string{text});
    }
    default: {
        std::string msg =
            fmt::format("unexpected token: {}", TokenName(toks.tok()));
        throw ExprParseError(msg);
    }
    }
}

enum {
    PrecAdd,
    PrecMul,
    PrecUnary,
};

} // namespace

double Literal::Eval(const Env &env) const {
    (void)&env;
    return value;
}

double VarRef::Eval(const Env &env) const {
    const auto r = env.find(name);
    if (r == env.end()) {
        std::string msg = fmt::format("undefined identifier '{}'", name);
        throw ExprEvalError(msg);
    }
    return r->second;
}

double UnExpr::Eval(const Env &env) const {
    double rhs = arg->Eval(env);
    switch (op) {
    case UnOp::Neg:
        return -rhs;
    }
    throw ExprEvalError("invalid UnExpr");
}

double BinExpr::Eval(const Env &env) const {
    double lhs = args[0]->Eval(env), rhs = args[1]->Eval(env);
    double result;
    switch (op) {
    case BinOp::Add:
        result = lhs + rhs;
        break;
    case BinOp::Sub:
        result = lhs - rhs;
        break;
    case BinOp::Mul:
        result = lhs * rhs;
        break;
    case BinOp::Div:
        if (rhs == 0.0) {
            throw ExprEvalError("division by zero");
        }
        result = lhs / rhs;
        break;
    default:
        throw ExprEvalError("invalid BinExpr");
    }
    if (!std::isfinite(result)) {
        throw ExprEvalError("expression overflowed");
    }
    return result;
}

std::string Expr::ToString() const {
    std::string s;
    Append(&s, PrecAdd);
    return s;
}

void Literal::Append(std::string *out, int prec) const {
    (void)prec;
    out->append(std::to_string(value));
}

void VarRef::Append(std::string *out, int prec) const {
    (void)prec;
    out->append(name);
}

void UnExpr::Append(std::string *out, int prec) const {
    (void)prec;
    switch (op) {
    case UnOp::Neg:
        out->append("-");
        break;
    }
    arg->Append(out, PrecUnary);
}

void BinExpr::Append(std::string *out, int prec) const {
    int op_prec;
    const char *op_text;
    switch (op) {
    case BinOp::Add:
        op_prec = PrecAdd;
        op_text = "+";
        break;
    case BinOp::Sub:
        op_prec = PrecAdd;
        op_text = "-";
        break;
    case BinOp::Mul:
        op_prec = PrecMul;
        op_text = "*";
        break;
    case BinOp::Div:
        op_prec = PrecMul;
        op_text = "/";
        break;
    default:
        throw ExprEvalError("invalid BinExpr");
    }
    if (prec > op_prec) {
        out->push_back('(');
    }
    args[0]->Append(out, op_prec);
    out->push_back(' ');
    out->append(op_text);
    out->push_back(' ');
    args[1]->Append(out, op_prec + 1);
    if (prec > op_prec) {
        out->push_back(')');
    }
}

Expr::Ref Expr::Parse(std::string_view text) {
    Tokenizer toks{text};
    toks.Next();
    Expr::Ref expr = ParseAdd(toks);
    if (toks.tok() != Token::End) {
        std::string msg =
            fmt::format("unexpected token: {}", TokenName(toks.tok()));
        throw ExprParseError(msg);
    }
    return expr;
}

std::string Expr::ParseIdent(std::string_view text) {
    Tokenizer toks{text};
    toks.Next();
    if (toks.tok() != Token::Ident) {
        std::string msg =
            fmt::format("expected identifier, got {}", TokenName(toks.tok()));
        throw ExprParseError(msg);
    }
    std::string_view ident{toks.text()};
    toks.Next();
    if (toks.tok() != Token::End) {
        std::string msg =
            fmt::format("unexpected token: {}", TokenName(toks.tok()));
        throw ExprParseError(msg);
    }
    return std::string{ident};
}

} // namespace util
