// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#pragma once
#include <cstdio>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace flag {

// Print an error message to the console and exit the program with status 2.
[[noreturn]] void FailUsage(std::string_view msg);

// The arguments passed to a program.
class ProgramArguments;

// =============================================================================
// Exception
// =============================================================================

class UsageError : public std::runtime_error {
public:
    explicit UsageError(const std::string &what);
    explicit UsageError(const char *what);
};

// =============================================================================
// Flags
// =============================================================================

// Whether an argument for the flag is required or possible.
enum class FlagArgument {
    None,
    Optional,
    Required,
};

// Base class for command-line flags.
class FlagBase {
public:
    virtual ~FlagBase();

    // Return whether this flag has an argument.
    virtual FlagArgument Argument() const = 0;

    // Default metavar for flags of this type.
    virtual const char *MetaVar() const;

    // Parse the flag. Throw UsageError if the flag or flag argument is invalid.
    virtual void Parse(std::optional<std::string_view> arg) = 0;
};

// String valued flag.
class String : public FlagBase {
    std::string *m_ptr;

public:
    explicit String(std::string *value) : m_ptr{value} {}

    FlagArgument Argument() const override;
    const char *MetaVar() const override;
    void Parse(std::optional<std::string_view> arg) override;
};

// Integer valued flag.
class Int : public FlagBase {
    int *m_ptr;

public:
    explicit Int(int *ptr) : m_ptr{ptr} {}

    FlagArgument Argument() const override;
    const char *MetaVar() const override;
    void Parse(std::optional<std::string_view> arg) override;
};

// Float32 valued flag.
class Float32 : public FlagBase {
    float *m_ptr;

public:
    explicit Float32(float *value) : m_ptr{value} {}

    FlagArgument Argument() const override;
    const char *MetaVar() const override;
    void Parse(std::optional<std::string_view> arg) override;
};

// Float64 valued flag.
class Float64 : public FlagBase {
    double *m_ptr;

public:
    explicit Float64(double *value) : m_ptr{value} {}

    FlagArgument Argument() const override;
    const char *MetaVar() const override;
    void Parse(std::optional<std::string_view> arg) override;
};

// Flag which sets a variable to a value if a flag appears.
template <typename T>
class SetValue : public FlagBase {
    T *m_ptr;
    T m_value;

public:
    SetValue(T *ptr, T value) : m_ptr{ptr}, m_value{value} {}
    ~SetValue() = default;

    FlagArgument Argument() const override { return FlagArgument::None; }
    void Parse(std::optional<std::string_view> arg) override {
        (void)&arg;
        *m_ptr = m_value;
    }
};

// =============================================================================
// Parser
// =============================================================================

// Types of positional arguments.
enum class PositionalType {
    Optional,
    Required,
    ZeroOrMore,
    OneOrMore,
};

// Command-line argument parser.
class Parser {
    struct Flag {
        std::shared_ptr<FlagBase> flag;
        std::string help;
        std::string metavar;
    };

    struct Positional {
        std::shared_ptr<FlagBase> flag;
        PositionalType type;
        std::string name;
        std::string help;
    };

    std::unordered_map<std::string, std::shared_ptr<const Flag>> m_flags;
    std::vector<Positional> m_positional;
    std::size_t m_position;
    bool m_positional_only;
    bool m_has_final_arg;
    void (*m_helpfn)(FILE *fp, Parser &fl);

public:
    Parser()
        : m_position{0},
          m_positional_only{false},
          m_has_final_arg{false},
          m_helpfn{nullptr} {}
    ~Parser();

    // Set the help function. If the function is set, -h and -help will call
    // this function and then exit the program with status 0.
    void SetHelp(void (*help)(FILE *fp, Parser &fl)) { m_helpfn = help; }

    // Print option help.
    void OptionHelp(FILE *fp);

    // Add a flag to the argument parser.
    template <typename F>
    void AddFlag(F &&flag, const char *name, const char *help,
                 const char *metavar = nullptr) {
        static_assert(std::is_base_of<FlagBase, F>::value,
                      "flag must be derived from FlagBase");
        AddFlagImpl(std::make_shared<F>(std::forward<F>(flag)), name, help,
                    metavar);
    }

    // Add a boolean-valued flag to the argument parser.
    //
    // The value will be true for -flag, -flag=true, flag=yes, flag=on, and
    // -flag=1. The value will be false for -no-flag, -flag=false, -flag=no,
    // flag=off, and -flag=0.
    void AddBoolFlag(bool *value, const char *name, const char *help);

    template <typename F>
    void AddPositional(F &&flag, PositionalType type, const char *name,
                       const char *help) {
        static_assert(std::is_base_of<FlagBase, F>::value,
                      "flag must be derived from FlagBase");
        AddPositionalImpl(std::make_shared<F>(std::forward<F>(flag)), type,
                          name, help);
    }

    // Parse all command-line arguments. Logs an error and exits with status 2
    // if there are any problems with the arguments. The arguments passed in
    // here should not include the program name.
    void Parse(int argc, char **argv);

    // Parse all command-line arguments, including the program name.
    void ParseMain(int argc, char **argv);

private:
    void ParseAll(ProgramArguments &args);
    void ParseNext(ProgramArguments &args);

    void AddFlagImpl(std::shared_ptr<FlagBase> flag, const char *name,
                     const char *help, const char *metavar);
    void AddPositionalImpl(std::shared_ptr<FlagBase> flag, PositionalType type,
                           const char *name, const char *help);
    void ParsePositional(const char *arg);
    int MinArgs() const;
};

} // namespace flag
