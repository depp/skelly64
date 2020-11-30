#pragma once

#include "tools/modelconvert/axes.hpp"

namespace modelconvert {

// Configuration for importing / rendering the mesh.
struct Config {
    // If true, the materials are given a primitive color equal to the
    // material’s diffuse color in the input model.
    bool use_primitive_color;
    // If true, vertex normals are added to the vertex data.
    bool use_normals;
    // If true, texture coordinates are added to the vertex data.
    bool use_texcoords;
    // If true, vertex colors are added to the vertex data.
    bool use_vertex_colors;
    // Number of fractional bits of precision for texture coordinates.
    int texcoord_bits;
    // The amount to scale the model data.
    float scale;
    // The order and sign of the axes.
    Axes axes;
    // If true, create animations.
    bool animate;
};

} // namespace modelconvert
