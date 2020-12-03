#pragma once

#include "tools/modelconvert/axes.hpp"

#include <array>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

struct aiScene;

namespace modelconvert {

struct Config;

class MeshError : public std::runtime_error {
public:
    MeshError(const char *msg) : runtime_error{msg} {}
    MeshError(const std::string &msg) : runtime_error{msg} {}
};

// Vertex attributes, other than position.
struct VertexAttr {
    std::array<int16_t, 2> texcoord;
    std::array<uint8_t, 4> color;
    std::array<int8_t, 3> normal;

    bool operator==(const VertexAttr &other) const {
        return texcoord == other.texcoord && color == other.color &&
               normal == other.normal;
    }

    bool operator!=(const VertexAttr &other) const { return !(*this == other); }
};

// An individual triangle in a mesh.
struct Triangle {
    int material;
    std::array<int, 3> vertex;
};

// A frame in a mesh animation.
struct AnimationFrame {
    // Time at which the frame is displayed, in seconds, where 0 is the start of
    // the animation.
    float time;

    // Index of the frame position data, in the frame array, to display at this
    // point in the animation.
    int data_index;
};

// A mesh animation.
struct Animation {
    // Duration of animation, in seconds.
    float duration;

    // List of frames, sorted ascending by time.
    std::vector<AnimationFrame> frame;
};

// A complete mesh.
struct Mesh {
    // Vertex attributes, excluding position.
    std::vector<VertexAttr> vertex;

    // Triangles.
    std::vector<Triangle> triangle;

    // Animations, and the associated frame data. Some may be null. Frame 0 is
    // the bind pose, it is always present.
    std::vector<std::unique_ptr<Animation>> animation;
    std::vector<std::vector<std::array<int16_t, 3>>> animation_frame;

    // Import a scene as a mesh.
    static Mesh Import(const Config &cfg, std::FILE *stats,
                       const aiScene *scene);
};

} // namespace modelconvert
