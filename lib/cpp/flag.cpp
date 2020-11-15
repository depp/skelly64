#include "tools/util/flag.hpp"

#include "tools/util/quote.hpp"

#include <cassert>
#include <cstring>

namespace flag {

UsageError::UsageError(const std::string &what) : std::runtime_error{what} {}
UsageError::UsageError(const char *what) : std::runtime_error{what} {}

FlagBase::~FlagBase() {}

String::String(std::string *value) : m_ptr{value} {}
String::~String() {}
bool String::HasArgument() const {
    return true;
}
void String::Parse(std::optional<std::string_view> arg) {
    assert(arg.has_value());
    *m_ptr = *arg;
}

Parser::~Parser() {}

void Parser::ParseAll(ProgramArguments &args) {
    while (!args.empty()) {
        ParseNext(args);
    }
}

namespace {

constexpr const char *UnexpectedArgument = "unexpected argument";

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

    // Strip the leading - or -- (equivalent).
    if (*p != '-') {
        throw MakeUsageError(UnexpectedArgument, arg);
    }
    p++;
    if (*p == '\0') {
        throw MakeUsageError(UnexpectedArgument, arg);
    } else if (*p == '-') {
        p++;
        if (*p == '\0') {
            throw MakeUsageError(UnexpectedArgument, arg);
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
    if (fl.HasArgument()) {
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
    } else {
        if (value.has_value()) {
            std::string flag = "-";
            flag.append(name);
            throw MakeUsageError("flag has unexpected parameter", flag);
        }
    }
    fl.Parse(value);
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

} // namespace flag
