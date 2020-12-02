#pragma once

#include "tools/modelconvert/gbi.hpp"

#include <vector>

namespace modelconvert {

struct Config;

namespace gbi {

// A compiled model.
struct Model {
    std::vector<Gfx> command;
    std::vector<Vtx> vertex;

    // Emit model as a model file.
    std::vector<uint8_t> Emit(const Config &cfg) const;
};

} // namespace gbi
} // namespace modelconvert
