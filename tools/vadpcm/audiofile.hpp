// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace vadpcm {

// Audio file formats.
enum class AudioFormat {
    Unknown,
    AIFF,
    AIFC,
};

// Return the audio format which uses the given file extension, or Unknown if no
// known format uses this extension.
AudioFormat AudioFormatForExtension(std::string_view extension);

// Contents of an audio file.
struct AudioFile {
private:
    enum class SampleFormat {
        None,
        S16,
        F32,
    };

    int m_sample_rate;
    int m_channel_count;
    std::vector<int16_t> m_data;

public:
    AudioFile() : m_sample_rate{0}, m_channel_count{0} {}

    int sample_rate() const { return m_sample_rate; }
    int channel_count() const { return m_channel_count; }
    std::vector<int16_t> &data() { return m_data; }
    const std::vector<int16_t> &data() const { return m_data; }

    // Set the rate and channel count.
    void SetInfo(int sample_rate, int channel_count) {
        m_sample_rate = sample_rate;
        m_channel_count = channel_count;
    }

    // Set the sample data.
    void SetData(std::vector<int16_t> &&data);

    // Load data from a file.
    static AudioFile Load(const std::string &path, AudioFormat format);
};

} // namespace vadpcm
