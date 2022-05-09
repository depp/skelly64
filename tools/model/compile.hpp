// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
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
