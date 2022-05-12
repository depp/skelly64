// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "lib/cpp/error.hpp"
#include "lib/cpp/log.hpp"
#include "lib/cpp/quote.hpp"
#include "tools/vadpcm/defs.hpp"

#include <cstdlib>
#include <cstring>
#include <fmt/format.h>

using util::Err;
using util::Error;

namespace vadpcm {
namespace {

struct Command {
    char name[8];
    int (*run)(int argc, char **argv);
    void (*help)(FILE *fp);
};

const Command kCommands[] = {
    {"decode", DecodeMain, DecodeHelp},
};

const Command *LookupCommand(std::string_view name) {
    for (const Command &cmd : kCommands) {
        if (name == cmd.name) {
            return &cmd;
        }
    }
    return nullptr;
}

void MainHelp(FILE *fp) {
    std::fputs(
        "Usage: vadpcm <command> [-h | -help] [<args>]\n"
        "       vadpcm [-h | -help | help] [<topic>]\n"
        "\n"
        "Commands:\n"
        "  decode  Decode a VADPCM audio file\n",
        fp);
}

void ExecHelp(int argc, char **argv) {
    if (argc >= 1) {
        std::string_view arg = argv[0];
        const Command *cmd = LookupCommand(arg);
        if (cmd == nullptr) {
            Err("unknown help topic: {}", util::Quote(arg));
            std::fputs("Topics are:", stderr);
            for (const Command &cmd : kCommands) {
                fmt::print(stderr, " {}", cmd.name);
            }
            std::fputc('\n', stderr);
            std::exit(2);
        }
        cmd->help(stdout);
    } else {
        MainHelp(stdout);
    }
}

} // namespace
} // namespace vadpcm

int main(int argc, char **argv) {
    using namespace vadpcm;
    try {
        if (argc == 1) {
            MainHelp(stdout);
            return 0;
        }
        std::string_view arg = argv[1];
        if (arg.size() > 0 && arg[0] == '-') {
            std::string_view opt;
            if (arg.size() > 1 && arg[1] == '-') {
                opt = arg.substr(2);
            } else {
                opt = arg.substr(1);
            }
            if (opt == "h" || opt == "help") {
                ExecHelp(argc - 2, argv + 2);
                return 0;
            }
            Err("unexpected option: {}", util::Quote(opt));
            std::fputs("Expected command or -h / -help.\n", stderr);
            std::exit(2);
        }
        if (arg == "help") {
            ExecHelp(argc - 2, argv + 2);
            return 0;
        }
        const Command *cmd = LookupCommand(arg);
        if (cmd == nullptr) {
            Err("unknown command: {}", util::Quote(arg));
            std::fputs("Commands are:", stderr);
            for (const Command &cmd : kCommands) {
                fmt::print(stderr, " {}", cmd.name);
            }
            std::fputc('\n', stderr);
            std::exit(2);
        }
        return cmd->run(argc - 2, argv + 2);
    } catch (Error &e) {
        fmt::print(stderr, "Error: {}\n", e.what());
        std::exit(1);
    }
    return 0;
}
