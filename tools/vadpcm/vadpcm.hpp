// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#pragma once

#include <cstdlib>
#include <string>
#include <utility>

#include "lib/vadpcm/vadpcm.h"

namespace vadpcm {

// Wrapper for vadpcm_codebook objects.
class Codebook {
public:
    Codebook() : m_ptr{nullptr} {}
    explicit Codebook(vadpcm_codebook *ptr) : m_ptr{ptr} {}
    Codebook(const Codebook &) = delete;
    Codebook(Codebook &&other) : m_ptr{other.m_ptr} { other.m_ptr = nullptr; }
    Codebook &operator=(const Codebook &) = delete;
    Codebook &operator=(Codebook &&other) {
        std::swap(m_ptr, other.m_ptr);
        return *this;
    }
    ~Codebook() { std::free(m_ptr); }

    operator vadpcm_codebook *() { return m_ptr; }
    operator bool() const { return m_ptr != nullptr; }
    bool operator!() const { return m_ptr == nullptr; }

private:
    vadpcm_codebook *m_ptr;
};

std::string VADPCMErrorMessage(vadpcm_error err);

} // namespace vadpcm
