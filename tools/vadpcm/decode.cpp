// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "lib/cpp/bswap.hpp"
#include "lib/cpp/error.hpp"
#include "lib/cpp/file.hpp"
#include "lib/cpp/flag.hpp"
#include "lib/cpp/log.hpp"
#include "lib/cpp/path.hpp"
#include "lib/cpp/quote.hpp"
#include "lib/vadpcm/vadpcm.h"
#include "tools/vadpcm/aiff.hpp"
#include "tools/vadpcm/vadpcm.hpp"

#include <fmt/format.h>

#include <cstdio>
#include <err.h>
#include <stdexcept>

using util::Err;
using util::Error;
using util::Read16;
using util::Read32;
using util::Read64;
using util::Warn;
using util::Write16;
using util::Write32;
using util::Write64;

namespace vadpcm {
namespace {

struct Args {
    std::string input;
    std::string output;
};

void Help(FILE *fp, flag::Parser &fl) {
    std::fputs("Usage: vadpcm decode <input.aifc> <output>\n\n", fp);
    fl.OptionHelp(fp);
}

void InitFlagParser(Args &args, flag::Parser &fl) {
    fl.SetHelp(Help);
    fl.AddPositional(flag::String(&args.input), flag::PositionalType::Required,
                     "input", "input AIFC file");
    fl.AddPositional(flag::String(&args.output), flag::PositionalType::Required,
                     "output", "output AIFF or AIFC file");
}

Args ParseArgs(int argc, char **argv) {
    std::string wd;
    {
        const char *dir = std::getenv("BUILD_WORKSPACE_DIRECTORY");
        if (dir != nullptr) {
            wd.assign(dir);
        }
    }
    Args args{};
    flag::Parser fl;
    InitFlagParser(args, fl);
    fl.Parse(argc, argv);
    return args;
}

Format FormatForPath(std::string_view path) {
    std::string_view ext = util::Extension(path);
    return ext == ".aifc" ? Format::AIFC : Format::AIFF;
}

} // namespace

int DecodeMain(int argc, char **argv) {
    Args args = ParseArgs(argc, argv);

    Format out_format = FormatForPath(args.output);

    AIFFReader input;
    input.Open(args.input);
    AIFFWriter output;
    output.Create(args.output, out_format);
    bool has_comm = false;
    bool has_codebook = false;
    vadpcm_codebook_spec cbspec{};
    std::vector<vadpcm_vector> cbvec;
    std::vector<uint8_t> temp;
    while (true) {
        ChunkHeader head;
        if (!input.NextChunk(&head)) {
            break;
        }
        switch (head.id) {
        case 'COMM': {
            if (has_comm) {
                throw util::Error(
                    input.ChunkMessage("multiple common chunks encountered"));
            }
            has_comm = true;
            CommonChunk comm;
            comm.Read(input);
            if (comm.compression_type != kCompressionVADPCM) {
                throw util::Error(fmt::format(
                    "{}: compression type is {} ({}), expected \"VAPC\"",
                    args.input, util::Quote(comm.compression_name),
                    comm.compression_type));
            }
            if (comm.num_channels != 1) {
                throw util::Error(fmt::format(
                    "{}: file has {} channels, only mono is supported",
                    args.input, comm.num_channels));
            }
            // comm.Dump(stderr);
            comm.sample_size = 16;
            comm.compression_type = kCompressionPCM;
            comm.compression_name = kCompressionPCMName;
            comm.Write(output);
        } break;
        case 'APPL': {
            input.ReadAll(&temp);
            const uint8_t *ptr = temp.data(), *end = ptr + temp.size();
            if (end - ptr < 4) {
                throw util::Error(input.ChunkMessage(
                    "chunk too small, must be at least 4 bytes"));
            }
            uint32_t sig = Read32(ptr);
            ptr += 4;
            if (sig == 'stoc') {
                std::string name;
                size_t adv = ReadPString(&name, ptr, end);
                if (adv == 0) {
                    throw util::Error(
                        input.ChunkMessage("unexpected end of chunk"));
                }
                ptr += adv;
                if (name == kAPPLCodebook) {
                    if (has_codebook) {
                        throw util::Error(input.ChunkMessage(
                            "multiple codebook chunks found"));
                    }
                    has_codebook = true;
                    size_t cboffset;
                    vadpcm_error err = vadpcm_read_codebook_aifc(
                        &cbspec, &cboffset, ptr, end - ptr);
                    if (err != 0) {
                        const char *msg = vadpcm_error_name(err);
                        if (msg == nullptr) {
                            throw util::Error(input.ChunkMessage(
                                "could not parse codebook: unknown error: {}",
                                static_cast<int>(err)));
                        }
                        throw util::Error(input.ChunkMessage(
                            "could not parse codebook: {}", msg));
                    }
                    ptr += cboffset;
                    int cbsize = cbspec.predictor_count * cbspec.order;
                    cbvec.resize(cbsize);
                    vadpcm_read_vectors(cbsize, ptr, cbvec.data());
                } else {
                    Warn("{}", input.ChunkMessage("unknown chunk type: {}",
                                                  util::Quote(name)));
                }
            } else {
                Warn("{}",
                     input.ChunkMessage("unknown application signature: {}",
                                        FourCC(sig)));
            }
        } break;
        case 'SSND': {
            if (!has_comm) {
                throw util::Error(
                    input.ChunkMessage("sound data found before common chunk"));
            }
            if (!has_codebook) {
                throw util::Error(input.ChunkMessage(
                    "sound data found before codebook chunk"));
            }
            input.ReadAll(&temp);
            const uint8_t *ptr = temp.data(), *end = ptr + temp.size();
            if (end - ptr < 8) {
                throw util::Error(input.ChunkMessage(
                    "chunk too small, must be at least 8 bytes"));
            }
            uint32_t offset = Read32(ptr);
            uint32_t block_size = Read32(ptr + 4);
            if (offset != 0 || block_size != 0) {
                throw util::Error(input.ChunkMessage(
                    "nonzero offset or block size not supported"));
            }
            ptr += 8;
            size_t size = end - ptr;
            size_t framecount = size / kVADPCMFrameByteSize;
            std::vector<int16_t> samp(framecount * kVADPCMFrameSampleCount);
            vadpcm_vector state;
            std::memset(&state, 0, sizeof(state));
            vadpcm_error err = vadpcm_decode(cbspec.predictor_count,
                                             cbspec.order, cbvec.data(), &state,
                                             framecount, samp.data(), ptr);
            if (err != 0) {
                throw util::Error(input.ChunkMessage("could not decode: {}",
                                                     VADPCMErrorMessage(err)));
            }
            {
                char buf[16];
                Write32(buf, 'SSND');
                Write32(buf + 4, framecount * kVADPCMFrameSampleCount * 2 + 8);
                std::memset(buf + 8, 0, 8);
                output.WriteChunkRaw(buf, sizeof(buf));
            }
            for (int16_t &x : samp) {
                x = util::BSwap16(x);
            }
            output.WriteChunkRaw(samp.data(), samp.size() * 2);
        } break;
        default:
            Warn("{}", input.ChunkMessage("unknown chunk type"));
        }
    }
    output.Commit();
    return 0;
}

void DecodeHelp(FILE *fp) {
    Args args{};
    flag::Parser fl;
    InitFlagParser(args, fl);
    Help(fp, fl);
}

} // namespace vadpcm
