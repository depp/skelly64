#pragma once

#include <cstdio>

#include "tools/model/model.hpp"

namespace modelconvert {

struct Config;
struct Mesh;

namespace gbi {

class DisplayList;

// Compile a mesh into a model usable by the engine.
Model CompileMesh(const Mesh &mesh, const Config &cfg, std::FILE *stats);

} // namespace gbi
} // namespace modelconvert
