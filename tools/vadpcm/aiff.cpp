// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "tools/vadpcm/aiff.hpp"

#include "lib/cpp/error.hpp"
#include "lib/cpp/log.hpp"
#include "lib/cpp/quote.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

using util::Read16;
using util::Read32;
using util::Read64;
using util::Write16;
using util::Write32;
using util::Write64;

namespace vadpcm {

std::string FourCC(uint32_t id) {
    auto buf = fmt::memory_buffer();
    buf.push_back('"');
    for (int i = 0; i < 4; i++) {
        int c = (id >> (24 - i * 8)) & 0xff;
        if (c < 32 || 126 < c) {
            fmt::format_to(std::back_inserter(buf), "\\x{:02x}", c);
        } else {
            if (c == '\\' || c == '"') {
                buf.push_back('\\');
            }
            buf.push_back(c);
        }
    }
    buf.push_back('"');
    return std::string(buf.data(), buf.size());
}

size_t ReadPString(std::string *str, const uint8_t *ptr, const uint8_t *end) {
    std::size_t size = end - ptr;
    if (ptr == end) {
        return 0;
    }
    size_t len = *ptr;
    if (len + 1 > size) {
        return 0;
    }
    str->assign(reinterpret_cast<const char *>(ptr + 1), len);
    size_t advance = len + 1;
    if (advance < size && (advance & 1) != 0) {
        advance++;
    }
    return advance;
}

void FileHeader::Read(const uint8_t *ptr) {
    id = Read32(ptr);
    size = Read32(ptr + 4);
    form_type = Read32(ptr + 8);
}

void FileHeader::Write(uint8_t *ptr) {
    Write32(ptr, id);
    Write32(ptr + 4, size);
    Write32(ptr + 8, form_type);
}

void ChunkHeader::Read(const uint8_t *ptr) {
    id = Read32(ptr);
    size = Read32(ptr + 4);
}

void ChunkHeader::Write(uint8_t *ptr) {
    Write32(ptr, id);
    Write32(ptr + 4, size);
}

void Extended::Read(const uint8_t *ptr) {
    exponent = Read16(ptr);
    fraction = Read64(ptr + 2);
}

void Extended::Write(uint8_t *ptr) {
    Write16(ptr, exponent);
    Write64(ptr + 2, fraction);
}

double Extended::GetDouble() {
    constexpr uint64_t hibit = static_cast<uint64_t>(1) << 63;
    uint64_t bits;
    int exp = (exponent & 0x7fff) - 16383 + 1023;
    if (exp <= 0 || (fraction & hibit) == 0) {
        // Denormals to zero.
        bits = 0;
    } else if (exp >= 2047) {
        // Overflow to infinity.
        bits = static_cast<uint64_t>(2047) << 52;
    } else {
        bits =
            (static_cast<uint64_t>(exp) << 52) | ((fraction & (~hibit)) >> 11);
    }
    if ((exp & 0x8000) != 0) {
        bits |= hibit;
    }
    union {
        uint64_t i;
        double d;
    } v;
    v.i = bits;
    return v.d;
}

const std::string_view kCompressionPCMName = "not compressed";
const std::string_view kCompressionVADPCMName = "VADPCM ~4-1";

std::string_view CompressionName(uint32_t ctype) {
    switch (ctype) {
    case kCompressionPCM:
        return kCompressionPCMName;
    case kCompressionVADPCM:
        return kCompressionVADPCMName;
    default:
        return std::string_view();
    }
}

constexpr size_t kCommonAIFFSize = 18;
constexpr size_t kCommonAIFCSize = 22; // Plus string.

void CommonChunk::Read(AIFFReader &file) {
    // Maximum possible size.
    uint8_t buf[kCommonAIFCSize + 256];
    uint32_t size = file.chunk_remaining();
    switch (file.format()) {
    case Format::AIFF:
        if (size < kCommonAIFFSize) {
            throw util::Error(
                file.ChunkMessage("chunk must be at least 18 bytes long"));
        }
        size = kCommonAIFFSize;
        break;
    case Format::AIFC:
        if (size < kCommonAIFCSize + 1) {
            throw util::Error(
                file.ChunkMessage("chunk must be at least 23 bytes long"));
        }
        if (size > sizeof(buf)) {
            size = sizeof(buf);
        }
        break;
    }
    file.ReadExact(buf, size);
    num_channels = Read16(buf);
    num_sample_frames = Read32(buf + 2);
    sample_size = Read16(buf + 6);
    sample_rate.Read(buf + 8);
    switch (file.format()) {
    case Format::AIFF:
        compression_type = kCompressionPCM;
        compression_name = kCompressionPCMName;
        break;
    case Format::AIFC:
        compression_type = Read32(buf + 18);
        size_t adv = ReadPString(&compression_name, buf + 22, buf + size - 22);
        if (adv == 0) {
            throw util::Error(file.ChunkMessage("unexpected end of chunk"));
        }
    }
}

void CommonChunk::Write(AIFFWriter &file) {
    // Maximum possible size.
    uint8_t buf[ChunkHeader::kSize + kCommonAIFCSize + 256];
    uint8_t *ptr = buf + ChunkHeader::kSize;
    if (file.format() == Format::AIFF && compression_type != kCompressionPCM) {
        throw std::logic_error(
            "cannot write compressed data to AIFF (must be AIFC)");
    }
    Write16(ptr, num_channels);
    Write32(ptr + 2, num_sample_frames);
    Write16(ptr + 6, sample_size);
    sample_rate.Write(ptr + 8);
    ptr += 18;
    if (file.format() == Format::AIFC) {
        Write32(ptr, compression_type);
        ptr += 4;
        if (compression_name.size() > 255) {
            throw std::range_error("compression name too long");
        }
        *ptr++ = compression_name.size();
        std::memcpy(ptr, compression_name.data(), compression_name.size());
        ptr += compression_name.size();
        if ((compression_name.size() & 1) == 0) {
            *ptr++ = 0;
        }
    }
    ChunkHeader head;
    head.id = 'COMM';
    head.size = ptr - (buf + ChunkHeader::kSize);
    head.Write(buf);
    file.WriteChunkRaw(buf, ptr - buf);
}

void CommonChunk::Dump(std::FILE *fp) {
    fmt::print(fp,
               "num_channels: {}\n"
               "num_sample_frames: {}\n"
               "sample_size: {}\n"
               "sample_rate: {}\n"
               "compression_type: {}\n"
               "comperssion_name: {}\n",
               num_channels, num_sample_frames, sample_size,
               sample_rate.GetDouble(), FourCC(compression_type),
               util::Quote(compression_name));
}

const std::string_view kAPPLCodebook = "VADPCMCODES";

void AIFFReader::Open(const std::string &name) {
    util::InputFile file;
    file.Open(name);

    uint8_t buf[FileHeader::kSize];
    file.ReadExact(buf, sizeof(buf));
    FileHeader head;
    head.Read(buf);
    if (head.id != 'FORM' ||
        (head.form_type != 'AIFC' && head.form_type != 'AIFF')) {
        throw util::Error(fmt::format("{}: not an AIFF or AIFC file", name));
    }
    if (head.size < 4) {
        throw util::Error(fmt::format("{}: invalid AIFF size", name));
    }
    m_file = std::move(file);
    m_format = head.form_type == 'AIFC' ? Format::AIFC : Format::AIFF;
    m_chunkid = 0;
    m_chunkoff = 0;
    m_pos = 12;
    m_end = head.size + 8;
    m_nextchunk = 12;
}

bool AIFFReader::NextChunk(ChunkHeader *head) {
    if (m_nextchunk >= m_end) {
        return false;
    }
    if (m_end - m_nextchunk < 8) {
        throw util::UnexpectedEOF(m_file.name());
    }
    if (m_pos != m_nextchunk) {
        m_file.Seek(m_nextchunk);
        m_pos = m_nextchunk;
    }
    uint8_t buf[ChunkHeader::kSize];
    m_file.ReadExact(buf, sizeof(buf));
    head->Read(buf);
    m_pos += ChunkHeader::kSize;
    m_chunkid = head->id;
    m_chunkoff = m_nextchunk;
    int64_t size = static_cast<int64_t>(head->size) + (head->size & 1);
    int64_t remaining = m_end - m_pos;
    if (size > remaining) {
        throw util::Error(ChunkMessage("chunk size is too large"));
    }
    m_nextchunk = m_pos + size;
    return true;
}

void AIFFReader::Skip(size_t amt) {
    if (amt == 0) {
        return;
    }
    uint64_t rem = m_nextchunk - m_pos;
    if (amt > rem) {
        throw util::Error(ChunkMessage("unexpected end of chunk"));
    }
    int64_t newpos = m_pos + amt;
    m_file.Seek(newpos);
    m_pos = newpos;
}

void AIFFReader::ReadExact(void *ptr, size_t amt) {
    uint64_t rem = m_nextchunk - m_pos;
    if (amt > rem) {
        throw util::Error(ChunkMessage("unexpected end of chunk"));
    }
    m_file.ReadExact(ptr, amt);
    m_pos += amt;
}

void AIFFReader::ReadAll(std::vector<uint8_t> *data) {
    data->resize(chunk_remaining());
    ReadExact(data->data(), chunk_remaining());
}

std::string AIFFReader::VChunkMessage(fmt::string_view format,
                                      fmt::format_args args) {
    auto buf = fmt::memory_buffer();
    fmt::format_to(std::back_inserter(buf),
                   "{}: in chunk {} at offset 0x{:x}: ", m_file.name(),
                   FourCC(m_chunkid), m_chunkoff);
    fmt::vformat_to(std::back_inserter(buf), format, args);
    return std::string(buf.data(), buf.size());
}

void AIFFWriter::Create(const std::string &name, Format format) {
    util::OutputFile file;
    file.Create(name);
    char head[FileHeader::kSize];
    memset(head, 0, sizeof(head));
    file.Write(head, sizeof(head));
    m_file = std::move(file);
    m_format = format;
}

void AIFFWriter::Commit() {
    FileHeader head;
    head.id = 'FORM';
    head.size = m_size - 8;
    head.form_type = m_format == Format::AIFF ? 'AIFF' : 'AIFC';
    uint8_t buf[FileHeader::kSize];
    head.Write(buf);
    m_file.Seek(0);
    m_file.Write(buf, sizeof(buf));
    m_file.Commit();
}

void AIFFWriter::WriteChunkRaw(const void *ptr, size_t size) {
    if ((size & 1) != 0) {
        throw std::invalid_argument("invalid chunk data length");
    }
    if (size > std::numeric_limits<uint32_t>::max() - m_size) {
        throw util::Error(fmt::format(
            "{}: AIFF data too long (over 4 GB limit)", m_file.name()));
    }
    m_file.Write(ptr, size);
    m_size += size;
}

} // namespace vadpcm
