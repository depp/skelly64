// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

struct aiMaterial;

namespace modelconvert {

struct Config;
struct Gfx;

template <typename T, std::size_t N>
struct VectorTraits {
    using Vector = std::array<T, N>;
    static Vector min_value() {
        const T value = std::numeric_limits<T>::min();
        Vector v;
        for (size_t i = 0; i < N; i++) {
            v[i] = value;
        }
        return v;
    }
    static Vector max_value() {
        const T value = std::numeric_limits<T>::max();
        Vector v;
        for (size_t i = 0; i < N; i++) {
            v[i] = value;
        }
        return v;
    }
    static Vector min(Vector x, Vector y) {
        Vector v;
        for (size_t i = 0; i < N; i++) {
            v[i] = std::min(x[i], y[i]);
        }
        return v;
    }
    static Vector max(Vector x, Vector y) {
        Vector v;
        for (size_t i = 0; i < N; i++) {
            v[i] = std::max(x[i], y[i]);
        }
        return v;
    }
};

// Maximum number of bones per vertex.
constexpr size_t MaxBones = 4;

// Maxmum bone weight, the fixed-point value of 1.0.
constexpr uint32_t MaxBoneWeight = 1 << 24;

// Bone weight for a vertex.
struct BoneWeight {
    uint32_t bone;
    uint32_t weight; // 8.24, see MaxBoneWeight.

    bool operator==(const BoneWeight &w) const;
    bool operator!=(const BoneWeight &w) const;
};

// All data that could be associated with a vertex in one structure.
struct GBIVertex {
    std::array<int16_t, 3> pos;
    std::array<int16_t, 2> texcoord;
    std::array<uint8_t, 3> color;
    uint8_t alpha;
    std::array<int8_t, 3> normal;
    std::array<BoneWeight, MaxBones> bone_weights;

    bool operator==(const GBIVertex &v) const;
    bool operator!=(const GBIVertex &v) const;
    uint32_t Hash() const;
};

struct HashVertex {
    uint32_t operator()(const GBIVertex &v) const { return v.Hash(); }
};

// Information about a material.
struct Material {
    // Primitive color.
    std::array<uint8_t, 4> rgba;

    // The default material.
    static Material Default();

    // Import a material from Assimp.
    static Material Import(const Config &cfg, aiMaterial *mat);
};

} // namespace modelconvert
