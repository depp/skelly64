// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "lib/cpp/flag.hpp"
#include "lib/cpp/log.hpp"
#include "lib/cpp/path.hpp"
#include "lib/cpp/quote.hpp"
#include "tools/vadpcm/aiff.hpp"
#include "tools/vadpcm/audiofile.hpp"
#include "tools/vadpcm/defs.hpp"

using util::Err;
using util::Warn;

namespace vadpcm {
namespace {

struct Args {
    std::string input;
    std::string output;
};

void Help(FILE *fp, flag::Parser &fl) {
    std::fputs("Usage: vadpcm decode <input> <output.aifc>\n\n", fp);
    fl.OptionHelp(fp);
}

void InitFlagParser(Args &args, flag::Parser &fl) {
    fl.SetHelp(Help);
    fl.AddPositional(flag::String(&args.input), flag::PositionalType::Required,
                     "input", "input audio file");
    fl.AddPositional(flag::String(&args.output), flag::PositionalType::Required,
                     "output", "output AIFC file");
}

Args ParseArgs(int argc, char **argv) {
    Args args{};
    flag::Parser fl;
    InitFlagParser(args, fl);
    fl.Parse(argc, argv);
    std::string_view ext = util::Extension(args.output);
    if (ext != ".aifc") {
        if (ext.empty()) {
            Warn("output file is missing extension \".aifc\"");
        } else {
            Warn(
                "output file has extension {}, "
                "but the correct extension is \".aifc\"",
                util::Quote(ext));
        }
    }
    return args;
}

} // namespace

int EncodeMain(int argc, char **argv) {
    Args args = ParseArgs(argc, argv);
    (void)&args;
    AudioFormat afmt = AudioFormatForExtension(util::Extension(args.input));
    AudioFile fl = AudioFile::Load(args.input, afmt);
    const std::vector<int16_t> &data = fl.data();
    fmt::print(stderr, "samples: {}\n", data.size());
    return 0;
}

void EncodeHelp(FILE *fp) {
    Args args{};
    flag::Parser fl;
    InitFlagParser(args, fl);
    Help(fp, fl);
}

} // namespace vadpcm