#pragma once

#include "tools/modelconvert/gbi.hpp"

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace modelconvert {
namespace gbi {

// A model of the vertex cache state.
class VertexCache {
    struct Entry;

public:
    explicit VertexCache(unsigned size);

    int size() const { return m_entries.size(); }

    // Get the vertex in a slot, or null if the slot is empty.
    const Vtx *Get(int cache_slot) const;
    Vtx *Get(int cache_slot);

    // Find a vertex with the given position, or return -1 if not present.
    int CachePos(std::array<int16_t, 3> pos) const;

    // Erase the given entry.
    void Erase(int cache_slot);

    // Clear the entire cache.
    void Clear();

    // Set the contents of the given entry.
    void Set(int cache_slot, const Vtx &v);

private:
    void EraseEntry(int cache_slot, Entry &e);

    struct Entry {
        bool valid;
        Vtx vertex;
    };

    struct HashPos {
        uint32_t operator()(const std::array<int16_t, 3> &p) const;
    };

    std::vector<Entry> m_entries;
    std::unordered_map<std::array<int16_t, 3>, int, HashPos> m_pos;
};

} // namespace gbi
} // namespace modelconvert
