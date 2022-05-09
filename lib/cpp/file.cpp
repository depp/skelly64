// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "lib/cpp/file.hpp"

#include "lib/cpp/error.hpp"

#include <errno.h>

namespace util {

void File::Set(std::FILE *file, std::string_view name) {
    m_name = name;
    if (m_file) {
        std::fclose(m_file);
    }
    m_file = file;
}

void File::Close() {
    int r = fclose(m_file);
    m_file = nullptr;
    std::string name;
    std::swap(name, m_name);
    if (r != 0) {
        throw IOError(name, "close", errno);
    }
}

void File::Seek(int64_t offset) {
    int r = fseek(m_file, offset, SEEK_SET);
    if (r != 0) {
        throw IOError(m_name, "seek", errno);
    }
}

void InputFile::Open(const std::string &name) {
    FILE *fp = fopen(name.c_str(), "rb");
    if (fp == nullptr) {
        throw IOError(name, "open", errno);
    }
    Set(fp, name);
}

void InputFile::ReadExact(void *ptr, size_t amt) {
    size_t pos = 0;
    while (pos < amt) {
        size_t r =
            std::fread(static_cast<char *>(ptr) + pos, 1, amt - pos, *this);
        if (r == 0) {
            if (std::feof(file())) {
                throw UnexpectedEOF(name());
            }
            throw IOError(name(), "read", errno);
        }
        pos += r;
    }
}

void OutputFile::Create(const std::string &name) {
    FILE *fp = fopen(name.c_str(), "wb");
    if (fp == nullptr) {
        throw IOError(name, "create", errno);
    }
    Set(fp, name);
}

void OutputFile::Commit() {
    File::Close();
}

void OutputFile::Write(const void *ptr, size_t size) {
    size_t r = fwrite(ptr, 1, size, file());
    if (r != size) {
        throw IOError(name(), "write", errno);
    }
}

} // namespace util
