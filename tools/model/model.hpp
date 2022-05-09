// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#pragma once

#include "tools/model/gbi.hpp"

#include <array>
#include <string_view>
#include <vector>

namespace modelconvert {

struct Config;

namespace gbi {

// A vertex in a frame of animation.
struct FrameVertex {
    std::array<int16_t, 3> pos;
    uint16_t pad;
};

// Vertex data for a frame of animation.
struct FrameData {
    std::vector<FrameVertex> pos;
};

// A single frame of an animation.
struct AnimationFrame {
    float time;
    int index;
};

// An animation sequence.
struct Animation {
    float duration;
    std::vector<AnimationFrame> frame;
};

// A compiled model.
struct Model {
    std::vector<std::vector<Gfx>> command; // Command list per material.
    std::vector<Vtx> vertex;
    std::vector<Animation> animation;
    std::vector<FrameData> frame;

    // Emit model as a model file.
    std::vector<uint8_t> EmitBinary(const Config &cfg) const;

    // Emit model as a source file.
    std::vector<uint8_t> EmitSource(const Config &cfg,
                                    std::string_view variable_name) const;
};

} // namespace gbi
} // namespace modelconvert
