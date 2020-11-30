#include "tools/modelconvert/mesh.hpp"

#include <fmt/core.h>

namespace modelconvert {

unsigned VertexSet::Add(const FVertex &v) {
    auto it = indexes.find(v.vert);
    if (it != indexes.end()) {
        return it->second;
    }
    unsigned index = vertexes.size();
    vertexes.emplace_back(v);
    indexes.emplace(v.vert, index);
    return index;
}

} // namespace modelconvert
