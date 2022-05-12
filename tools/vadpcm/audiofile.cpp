// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "tools/vadpcm/audiofile.hpp"
#include "lib/cpp/bswap.hpp"
#include "lib/cpp/error.hpp"
#include "lib/cpp/log.hpp"
#include "lib/cpp/quote.hpp"
#include "tools/vadpcm/aiff.hpp"

#include <fmt/format.h>

#include <cassert>
#include <cmath>

using util::Error;
using util::Warn;

namespace vadpcm {

namespace {

constexpr size_t kBufferSize = 16 * 1024;

// =============================================================================
// Sample formats
// =============================================================================

struct FormatS8 {
    static constexpr size_t kSampleSize = 1;
    static void Convert(const uint8_t *src, int16_t *dest, size_t n,
                        util::Endian endian) {
        (void)endian;
        for (size_t i = 0; i < n; i++) {
            dest[i] = (int8_t)src[i] << 8;
        }
    }
};

struct FormatS24 {
    static constexpr size_t kSampleSize = 3;
    static void Convert(const uint8_t *src, int16_t *dest, size_t n,
                        util::Endian endian) {
        switch (endian) {
        case util::Endian::Big:
            for (size_t i = 0; i < n; i++) {
                dest[i] = ((int8_t)src[i * 3] << 8) | src[i * 3 + 1];
            }
            break;
        case util::Endian::Little:
            for (size_t i = 0; i < n; i++) {
                dest[i] = ((int8_t)src[i * 3 + 2] << 8) | src[i * 3 + 1];
            }
            break;
        }
    }
};

// =============================================================================
// AIFF
// =============================================================================

void LoadAIFFSamplesS16(AIFFReader &input, AudioFile *fl, uint32_t sample_count,
                        util::Endian endian) {
    std::vector<int16_t> data;
    data.resize(sample_count);
    input.ReadExact(data.data(), sample_count * 2);
    if (IsSwapped(endian)) {
        util::BSwap16Array(data.data(), data.size());
    }
    fl->SetData(std::move(data));
}

template <typename Format>
void LoadAIFFSamples(AIFFReader &input, AudioFile *fl, uint32_t sample_count,
                     util::Endian endian) {
    std::vector<int16_t> data;
    data.resize(sample_count);
    int16_t *outptr = data.data();

    std::vector<uint8_t> temp;
    temp.resize(std::min(kBufferSize, sample_count * Format::kSampleSize));
    uint8_t *buf = temp.data();
    size_t bufsize = temp.size();
    size_t bufsamples = bufsize / Format::kSampleSize;
    size_t remaining = sample_count;
    while (remaining > 0) {
        size_t req = std::min(remaining, bufsamples);
        input.ReadExact(temp.data(), req * Format::kSampleSize);
        Format::Convert(buf, outptr, req, endian);
        outptr += req;
        remaining -= req;
    }

    fl->SetData(std::move(data));
}

AudioFile LoadAIFF(const std::string &path) {
    AIFFReader input;
    input.Open(path);
    AudioFile fl;
    uint32_t channel_count = 0;
    uint32_t frame_count = 0;
    bool has_comm = false;
    bool has_ssnd = false;
    void (*loader)(AIFFReader & input, AudioFile * fl, uint32_t sample_count,
                   util::Endian endian) = nullptr;
    util::Endian endian = util::Endian::Big;
    while (true) {
        ChunkHeader head;
        if (!input.NextChunk(&head)) {
            break;
        }
        switch (head.id) {
        case 'COMM': {
            if (has_comm) {
                throw Error(input.ChunkMessage("multiple common chunks"));
            }
            has_comm = true;
            CommonChunk comm;
            comm.Read(input);
            double rate = comm.sample_rate.GetDouble();
            channel_count = comm.num_channels;
            frame_count = comm.num_sample_frames;
            if (!std::isfinite(rate)) {
                throw Error(input.ChunkMessage("invalid sample rate"));
            }
            if (rate < 1.0 || std::numeric_limits<int>::max() < rate) {
                throw Error(input.ChunkMessage(
                    "sample rate is out of range: {}", rate));
            }
            switch (comm.compression_type) {
            case kCompressionPCM:
                if (comm.sample_size <= 8) {
                    loader = LoadAIFFSamples<FormatS8>;
                } else if (comm.sample_size <= 16) {
                    loader = LoadAIFFSamplesS16;
                } else if (comm.sample_size <= 24) {
                    loader = LoadAIFFSamples<FormatS24>;
                } else {
                    throw Error(
                        fmt::format("{}: unsupported sample size: {} bits, "
                                    "supported sizes are 1..24 bits",
                                    path, comm.sample_size));
                }
                break;
            default:
                throw util::Error(
                    fmt::format("{}: unsupported compression type {} ({}), "
                                "supported types are: NONE",
                                path, util::Quote(comm.compression_name),
                                comm.compression_type));
            }
        } break;
        case 'SSND': {
            if (!has_comm) {
                throw util::Error(
                    input.ChunkMessage("sound data found before common chunk"));
            }
            if (has_ssnd) {
                throw util::Error(
                    input.ChunkMessage("multiple sound data chunks"));
            }
            has_ssnd = true;
            assert(loader != nullptr);
            uint32_t offset;
            {
                char tmp[8];
                input.ReadExact(tmp, 8);
                offset = util::Read32(tmp);
            }
            input.Skip(offset);
            loader(input, &fl, frame_count * channel_count, endian);
        } break;
        default:
            Warn("ignoring chunk: {}", FourCC(head.id));
        }
    }
    if (!has_ssnd) {
        throw util::Error(fmt::format("{}: no sound data found", path));
    }
    return fl;
}

struct FormatExtension {
    char extension[8];
    AudioFormat format;
};

const FormatExtension FormatExtensions[] = {
    {".aiff", AudioFormat::AIFF},
    {".aifc", AudioFormat::AIFC},
};

} // namespace

AudioFormat AudioFormatForExtension(std::string_view extension) {
    for (const FormatExtension &e : FormatExtensions) {
        if (extension == e.extension) {
            return e.format;
        }
    }
    return AudioFormat::Unknown;
}

void AudioFile::SetData(std::vector<int16_t> &&data) {
    m_data = std::move(data);
}

AudioFile AudioFile::Load(const std::string &path, AudioFormat format) {
    switch (format) {
    case AudioFormat::Unknown:
        throw Error("unknown audio format");
    case AudioFormat::AIFF:
        return LoadAIFF(path);
    case AudioFormat::AIFC:
        return LoadAIFF(path);
    }
    throw Error("bad audio format");
}

} // namespace vadpcm
