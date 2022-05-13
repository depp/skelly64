// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#pragma once

#include "lib/cpp/bswap.hpp"
#include "lib/cpp/error.hpp"
#include "lib/cpp/file.hpp"

#include <fmt/format.h>

#include <stdint.h>
#include <string>
#include <vector>

namespace vadpcm {

// AIFF formats.
enum class Format {
    AIFF,
    AIFC,
};

class AIFFReader;
class AIFFWriter;

class ChunkError : public util::Error {
public:
    using Error::Error;
};

// Format a four-character code as ASCII text.
std::string FourCC(uint32_t id);

// Read a 2-byte aligned Pascal string. Returns the number of bytes read on
// success. Returns 0 on error.
size_t ReadPString(std::string *str, const uint8_t *ptr, const uint8_t *end);

// The header of an AIFF or AIFC file.
struct FileHeader {
    uint32_t id;
    uint32_t size;
    uint32_t form_type;

    static constexpr size_t kSize = 12;
    void Read(const uint8_t *ptr);
    void Write(uint8_t *ptr);
};

// The header of a chunk in an AIFF file.
struct ChunkHeader {
    uint32_t id;
    uint32_t size;

    static constexpr size_t kSize = 8;
    void Read(const uint8_t *ptr);
    void Write(uint8_t *ptr);
};

// An 80-bit extended precision floating-point number.
struct Extended {
    uint16_t exponent;
    uint64_t fraction;

    void Read(const uint8_t *ptr);
    void Write(uint8_t *ptr);
    double GetDouble();
};

// Compression type for PCM.
constexpr uint32_t kCompressionPCM = 'NONE';

// Compression type for VADPCM.
constexpr uint32_t kCompressionVADPCM = 'VAPC';

// Descriptive name for PCM compression.
extern const std::string_view kCompressionPCMName;

// Descriptive name for VADPCM compression.
extern const std::string_view kCompressionVADPCMName;

// Return the descriptive name of the given compression type.
std::string_view CompressionName(uint32_t ctype);

// AIFF COMM chunk (section 4.0).
struct CommonChunk {
    int16_t num_channels;
    uint32_t num_sample_frames;
    int16_t sample_size;
    Extended sample_rate;
    uint32_t compression_type;
    std::string compression_name;

    void Read(AIFFReader &file);
    void Write(AIFFWriter &file);
    void Dump(std::FILE *fp);
};

// Codebook APPL chunk name.
extern const std::string_view kAPPLCodebook;

// Wrapper for reading an AIFF file.
class AIFFReader {
public:
    AIFFReader()
        : m_format{Format::AIFF},
          m_chunkid{0},
          m_chunkoff{0},
          m_pos{0},
          m_end{0},
          m_nextchunk{0} {}

    // Open the given AIFF file and read the file header.
    void Open(const std::string &name);

    // Read the next chunk in the file. Return false if there are no more
    // chunks.
    bool NextChunk(ChunkHeader *head);

    // Skip the given number of bytes from a chunk. Throw an error on failure.
    void Skip(size_t amt);

    // Read the given number of bytes from a chunk, and throw an error on
    // failure.
    void ReadExact(void *ptr, size_t amt);

    // Read the rest of the chunk.
    void ReadAll(std::vector<uint8_t> *data);

    // Create a diagnastic message for the most recently read chunk.
    template <typename S, typename... Args>
    std::string ChunkMessage(const S &format, Args &&...args) {
        return VChunkMessage(format,
                             fmt::make_args_checked<Args...>(format, args...));
    }

    // Create an error for the most recently read chunk.
    std::string VChunkMessage(fmt::string_view format, fmt::format_args args);

    Format format() const { return m_format; }

    uint32_t chunk_remaining() const { return m_nextchunk - m_pos; }

private:
    util::InputFile m_file;

    Format m_format;

    uint32_t m_chunkid;
    int64_t m_chunkoff;

    int64_t m_pos;
    int64_t m_end;
    int64_t m_nextchunk;
};

// Wrapper for writing an AIFF file.
class AIFFWriter {
public:
    AIFFWriter() : m_format{Format::AIFF} {}

    // Create a file with the given name.
    void Create(const std::string &name, Format format);

    // Finish writing the file.
    void Commit();

    // Write a raw chunk to the file.
    void WriteChunkRaw(const void *ptr, size_t size);

    Format format() const { return m_format; }

private:
    util::OutputFile m_file;

    Format m_format;
    uint32_t m_size;
};

} // namespace vadpcm
