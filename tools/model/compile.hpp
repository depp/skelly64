#pragma once

#include <cstdio>

namespace modelconvert {

struct Config;
struct Mesh;

namespace gbi {

class DisplayList;

// Compile a mesh and append it to a display list.
void CompileMesh(DisplayList *dl, const Mesh &mesh, const Config &cfg,
                 std::FILE *stats);

} // namespace gbi
} // namespace modelconvert
