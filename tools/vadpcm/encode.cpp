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
#include "tools/vadpcm/audiofile.hpp"
#include "tools/vadpcm/defs.hpp"
#include "tools/vadpcm/stats.hpp"
#include "tools/vadpcm/vadpcm.hpp"

#include <fmt/format.h>

#include <cassert>
#include <cmath>
#include <stdexcept>

using util::Err;
using util::Warn;
using util::Write16;
using util::Write32;

namespace vadpcm {
namespace {

struct Args {
    std::string input;
    std::string output;
    int predictor_count = 4;
    bool show_stats = false;
    std::string stats_file;
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
    fl.AddFlag(flag::Int(&args.predictor_count), "predictors",
               "encode with a codebook containing N predictors, "
               "1 <= N <= 16, default 4",
               "N");
    fl.AddBoolFlag(&args.show_stats, "show-stats", "show encoding statistics");
    fl.AddFlag(flag::String(&args.stats_file), "stats-out",
               "write stats in JSON format to file", "file");
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
    if (args.predictor_count < 1 ||
        kVADPCMMaxPredictorCount < args.predictor_count) {
        util::Err("flag -predictors={} is outside the range 1..16",
                  args.predictor_count);
        std::exit(2);
    }
    return args;
}

} // namespace

int EncodeMain(int argc, char **argv) {
    Args args = ParseArgs(argc, argv);

    // Read input. Pad with zeroes.
    AudioFormat afmt = AudioFormatForExtension(util::Extension(args.input));
    AudioFile fl = AudioFile::Load(args.input, afmt);
    std::vector<int16_t> &input = fl.data();
    const size_t frame_count =
        (input.size() + kVADPCMFrameSampleCount - 1) / kVADPCMFrameSampleCount;
    const size_t sample_count = frame_count * kVADPCMFrameSampleCount;
    input.resize(sample_count, 0);

    // Encode as VADPCM.
    vadpcm_params params{};
    params.predictor_count = args.predictor_count;
    std::vector<vadpcm_vector> codebook(kVADPCMEncodeOrder *
                                        args.predictor_count);
    size_t encoded_size = frame_count * kVADPCMFrameByteSize;
    std::vector<uint8_t> encoded((encoded_size + 1) & ~(size_t)1);
    if (!encoded.empty()) {
        encoded.back() = 0;
    }
    size_t scratch_size = vadpcm_encode_scratch_size(frame_count);
    void *scratch = std::malloc(scratch_size);
    if (scratch == nullptr) {
        throw std::bad_alloc();
    }
    vadpcm_error err = vadpcm_encode(&params, codebook.data(), frame_count,
                                     encoded.data(), input.data(), scratch);
    std::free(scratch);
    if (err != 0) {
        util::Err("could not encode VADPCM: {}", VADPCMErrorMessage(err));
        return 1;
    }

    // Write to file.
    AIFFWriter output;
    output.Create(args.output, Format::AIFC);
    {
        CommonChunk comm{};
        comm.num_channels = 1;
        comm.num_sample_frames = sample_count;
        comm.sample_size = 16;
        comm.sample_rate =
            Extended::FromDouble(static_cast<double>(fl.sample_rate()));
        comm.compression_type = kCompressionVADPCM;
        comm.compression_name = kCompressionVADPCMName;
        comm.Write(output);
    }
    {
        constexpr size_t base_size = 8 + 16 + 6;
        constexpr size_t predictor_size =
            kVADPCMEncodeOrder * kVADPCMVectorSampleCount * 2;
        size_t size =
            base_size + args.predictor_count * kVADPCMEncodeOrder * 16;
        char buf[base_size + predictor_size * kVADPCMMaxPredictorCount], *ptr;
        ptr = buf;
        Write32(ptr, 'APPL');
        Write32(ptr + 4, size - 8);
        ptr += 8;
        memcpy(ptr, "stoc\x0bVADPCMCODES", 16);
        ptr += 16;
        Write16(ptr, 1); // Version
        Write16(ptr + 2, kVADPCMEncodeOrder);
        Write16(ptr + 4, args.predictor_count);
        ptr += 6;
        for (const vadpcm_vector &v : codebook) {
            for (int i = 0; i < kVADPCMVectorSampleCount; i++) {
                Write16(ptr + 2 * i, v.v[i]);
            }
            ptr += 2 * kVADPCMVectorSampleCount;
        }
        assert(ptr == buf + size);
        output.WriteChunkRaw(buf, size);
    }
    {
        char buf[8 + 8];
        size_t size = kVADPCMFrameByteSize * frame_count + 8;
        Write32(buf, 'SSND');
        Write32(buf + 4, size);
        memset(buf + 8, 0, 8);
        output.WriteChunkRaw(buf, 16);
    }
    output.WriteChunkRaw(encoded.data(), encoded.size());
    output.Commit();

    if (args.show_stats || !args.stats_file.empty()) {
        std::vector<int16_t> decoded(sample_count);
        vadpcm_vector state{};
        err = vadpcm_decode(args.predictor_count, kVADPCMEncodeOrder,
                            codebook.data(), &state, frame_count,
                            decoded.data(), encoded.data());
        if (err != 0) {
            util::Err("could not decode encoded VADCM: {}",
                      VADPCMErrorMessage(err));
            return 1;
        }
        Stats st = Stats::Calculate(sample_count, input.data(), decoded.data());
        if (args.show_stats) {
            const double sigdb = 10.0 * std::log10(st.signal);
            const double noisedb = 10.0 * std::log10(st.noise);
            fmt::print(
                "Signal level:       {:5.1f} dB\n"
                "Noise level:        {:5.1f} dB\n"
                "Signal-noise ratio: {:5.1f} dB\n",
                sigdb, noisedb, sigdb - noisedb);
        }
        if (!args.stats_file.empty()) {
            util::OutputFile out;
            out.Create(args.stats_file);
            fmt::print(out.file(),
                       "{{\n"
                       "  \"signalLevel\": {:.7e},\n"
                       "  \"errorLevel\": {:.7e}\n"
                       "}}\n",
                       std::sqrt(st.signal), std::sqrt(st.noise));
        }
    }

    return 0;
}

void EncodeHelp(FILE *fp) {
    Args args{};
    flag::Parser fl;
    InitFlagParser(args, fl);
    Help(fp, fl);
}

} // namespace vadpcm