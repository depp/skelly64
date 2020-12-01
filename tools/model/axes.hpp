#pragma once

#include <assimp/matrix4x4.h>

#include <array>
#include <string>
#include <string_view>

namespace modelconvert {

// Axis orientation. Contains a permutation and sign for three axes.
class Axes {
public:
    Axes() : negate{{false, false, false}}, index{{0, 1, 2}} {}

    template <typename T>
    std::array<T, 3> Apply(std::array<T, 3> vec) const {
        return std::array<T, 3>{{
            negate[0] ? -vec[index[0]] : vec[index[0]],
            negate[1] ? -vec[index[1]] : vec[index[1]],
            negate[2] ? -vec[index[2]] : vec[index[2]],
        }};
    }

    aiMatrix4x4 ToMatrix() const;

    std::string ToString() const;
    static Axes Parse(std::string_view s);

private:
    std::array<bool, 3> negate;
    std::array<int, 3> index;
};

} // namespace modelconvert
