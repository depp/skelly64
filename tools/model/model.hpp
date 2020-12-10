#pragma once

#include "tools/modelconvert/gbi.hpp"

#include <array>
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
    std::vector<uint8_t> Emit(const Config &cfg) const;
};

} // namespace gbi
} // namespace modelconvert
