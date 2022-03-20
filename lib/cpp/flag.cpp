#include "lib/cpp/flag.hpp"

#include "lib/cpp/quote.hpp"

#include <fmt/core.h>

#include <cassert>
#include <cstring>

namespace flag {

UsageError::UsageError(const std::string &what) : std::runtime_error{what} {}
UsageError::UsageError(const char *what) : std::runtime_error{what} {}

FlagBase::~FlagBase() {}

// =============================================================================

FlagArgument String::Argument() const {
    return FlagArgument::Required;
}
void String::Parse(std::optional<std::string_view> arg) {
    assert(arg.has_value());
    *m_ptr = *arg;
}

// =============================================================================

FlagArgument Int::Argument() const {
    return FlagArgument::Required;
}
void Int::Parse(std::optional<std::string_view> arg) {
    assert(arg.has_value());
    std::string tmp{*arg};
    size_t pos;
    int value;
    try {
        value = std::stoi(tmp, &pos);
    } catch (std::invalid_argument &ex) {
        throw UsageError("expected an integer");
    } catch (std::out_of_range &ex) { throw UsageError("integer too large"); }
    if (pos != tmp.size()) {
        throw UsageError("expected an integer");
    }
    *m_ptr = value;
}

// =============================================================================

FlagArgument Float32::Argument() const {
    return FlagArgument::Required;
}
void Float32::Parse(std::optional<std::string_view> arg) {
    assert(arg.has_value());
    std::string tmp{*arg};
    size_t pos;
    float value;
    try {
        value = std::stof(tmp, &pos);
    } catch (std::invalid_argument &ex) {
        throw UsageError("expected a floating-point value");
    } catch (std::out_of_range &ex) {
        throw UsageError("floating-point value too large");
    }
    if (pos != tmp.size()) {
        throw UsageError("expected a floating-point value");
    }
    *m_ptr = value;
}

// =============================================================================

FlagArgument Float64::Argument() const {
    return FlagArgument::Required;
}
void Float64::Parse(std::optional<std::string_view> arg) {
    assert(arg.has_value());
    std::string tmp{*arg};
    size_t pos;
    double value;
    try {
        value = std::stod(tmp, &pos);
    } catch (std::invalid_argument &ex) {
        throw UsageError("expected a floating-point value");
    } catch (std::out_of_range &ex) {
        throw UsageError("floating-point value too large");
    }
    if (pos != tmp.size()) {
        throw UsageError("expected a floating-point value");
    }
    *m_ptr = value;
}

// =============================================================================

namespace {

struct BoolStr {
    char text[7];
    bool value;
};

const BoolStr BoolStrs[] = {
    {"false", false}, {"true", true}, {"no", false}, {"yes", true},
    {"off", false},   {"on", true},   {"0", false},  {"1", true},
};

// Boolean valued flag.
class Bool : public FlagBase {
    bool *m_ptr;

public:
    explicit Bool(bool *value) : m_ptr{value} {}

    FlagArgument Argument() const override { return FlagArgument::Optional; }
    void Parse(std::optional<std::string_view> arg) override {
        bool value = true;
        if (arg.has_value()) {
            bool ok = false;
            for (const BoolStr &s : BoolStrs) {
                if (*arg == s.text) {
                    value = s.value;
                    ok = true;
                    break;
                }
            }
            if (!ok) {
                throw UsageError("invalid value for boolean flag");
            }
        }
        *m_ptr = value;
    }
};

} // namespace

// =============================================================================

Parser::~Parser() {}

void Parser::ParseAll(ProgramArguments &args) {
    while (!args.empty()) {
        ParseNext(args);
    }
    if (m_position < m_positional.size()) {
        PositionalType last_type = m_positional[m_position].type;
        bool ok;
        switch (last_type) {
        case PositionalType::Required:
            ok = false;
            break;
        case PositionalType::OneOrMore:
            ok = m_has_final_arg;
            break;
        default:
            ok = true;
            break;
        }
        if (!ok) {
            std::string msg =
                fmt::format("at least {} arguments expected", MinArgs());
            throw UsageError(msg);
        }
    }
}

int Parser::MinArgs() const {
    int count = 0;
    for (const Positional &info : m_positional) {
        switch (info.type) {
        case PositionalType::Optional:
        case PositionalType::ZeroOrMore:
            return count;
        case PositionalType::Required:
            count++;
            break;
        case PositionalType::OneOrMore:
            count++;
            return count;
        }
    }
    return count;
}

namespace {

UsageError MakeUsageError(std::string_view msg, std::string_view arg) {
    std::string s;
    s.append(msg);
    s.append(": ");
    s.append(util::Quote(arg));
    throw UsageError(s);
}

} // namespace

void Parser::ParseNext(ProgramArguments &args) {
    const char *arg = args.arg(), *p = arg;
    if (arg == nullptr) {
        throw std::logic_error("no arguments");
    }
    args.Next();

    if (m_positional_only || *p != '-') {
        ParsePositional(arg);
        return;
    }

    // Strip the leading - or -- (equivalent).
    p++;
    if (*p == '\0') {
        ParsePositional(arg);
        return;
    } else if (*p == '-') {
        p++;
        if (*p == '\0') {
            m_positional_only = true;
            return;
        }
    }

    // Split -name=argument into name and argument.
    std::string_view name;
    std::optional<std::string_view> value;
    const char *eq = std::strchr(p, '=');
    if (eq != nullptr) {
        if (p == eq) {
            throw MakeUsageError("invalid flag", arg);
        }
        name = std::string_view(p, eq - p);
        value = std::string_view{eq + 1};
    } else {
        name = std::string_view{p};
        value = std::nullopt;
    }

    // Find the flag description.
    auto it = m_flags.find(std::string(name));
    if (it == m_flags.end()) {
        std::string flag = "-";
        flag.append(name);
        throw MakeUsageError("unknown flag", flag);
    }
    FlagBase &fl = *it->second->flag;

    // Parse the flag itself.
    switch (fl.Argument()) {
    case FlagArgument::Required:
        if (!value.has_value()) {
            arg = args.arg();
            if (arg == nullptr) {
                std::string flag = "-";
                flag.append(name);
                throw MakeUsageError("flag is missing required parameter",
                                     flag);
            }
            args.Next();
            value = std::string_view{arg};
        }
        break;
    case FlagArgument::None:
        if (value.has_value()) {
            std::string flag = "-";
            flag.append(name);
            throw MakeUsageError("flag has unexpected parameter", flag);
        }
    case FlagArgument::Optional:
        break;
    }

    fl.Parse(value);
}

void Parser::ParsePositional(const char *arg) {
    if (m_position >= m_positional.size()) {
        throw MakeUsageError("unexpected argument", arg);
    }
    Positional &info = m_positional[m_position];
    info.flag->Parse(std::string_view(arg));
    switch (info.type) {
    case PositionalType::Optional:
    case PositionalType::Required:
        m_position++;
        break;
    default:
        m_has_final_arg = true;
        break;
    }
}

void Parser::AddFlagImpl(std::shared_ptr<FlagBase> flag, const char *name,
                         const char *help, const char *metavar) {
    auto r = m_flags.emplace(name, nullptr);
    if (!r.second) {
        throw std::logic_error("duplicate flag");
    }
    std::shared_ptr<Flag> fp{std::make_shared<Flag>()};
    Flag &f = *fp;
    f.flag = std::move(flag);
    if (help != nullptr) {
        f.help.assign(help);
    }
    if (metavar != nullptr) {
        f.metavar.assign(metavar);
    }
    r.first->second = std::move(fp);
}

void Parser::AddBoolFlag(bool *value, const char *name, const char *help) {
    std::string pos_name = name;
    std::string neg_name = "no-";
    neg_name.append(name);

    std::shared_ptr<Flag> fpos{std::make_shared<Flag>()};
    std::shared_ptr<Flag> fneg{std::make_shared<Flag>()};
    fpos->flag = std::make_shared<Bool>(value);
    if (help != nullptr) {
        fpos->help.assign(help);
    }
    fneg->flag = std::make_shared<SetValue<bool>>(value, false);

    auto r = m_flags.emplace(pos_name, nullptr);
    if (!r.second) {
        throw std::logic_error("duplicate flag");
    }
    r.first->second = std::move(fpos);
    r = m_flags.emplace(neg_name, nullptr);
    if (!r.second) {
        throw std::logic_error("duplicate flag");
    }
    r.first->second = std::move(fneg);
}

void Parser::AddPositionalImpl(std::shared_ptr<FlagBase> flag,
                               PositionalType type, const char *name,
                               const char *help) {
    if (!m_positional.empty()) {
        PositionalType last_type = m_positional.back().type;
        switch (last_type) {
        case PositionalType::Optional:
            if (type == PositionalType::Required ||
                type == PositionalType::OneOrMore) {
                throw std::logic_error(
                    "cannot add required positional argument after optional "
                    "argument");
            }
            break;
        case PositionalType::ZeroOrMore:
        case PositionalType::OneOrMore:
            throw std::logic_error(
                "cannot add positional argument after ZeroOrMore or OneOrMore "
                "argument");
            break;
        default:
            break;
        }
    }
    m_positional.push_back(Positional{
        std::move(flag),
        type,
        name,
        help,
    });
}

} // namespace flag
