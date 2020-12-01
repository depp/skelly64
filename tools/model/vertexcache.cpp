#include "tools/modelconvert/vertexcache.hpp"

#include "tools/util/hash.hpp"

#include <stdexcept>

namespace modelconvert {
namespace gbi {

uint32_t VertexCache::HashPos::operator()(
    const std::array<int16_t, 3> &p) const {
    util::Murmur3 h = util::Murmur3::Initial(0);
    h.Update(p[0]);
    h.Update(p[1]);
    h.Update(p[2]);
    return h.Hash();
}

VertexCache::VertexCache(unsigned size) : m_entries(size, Entry{}) {}

const Vtx *VertexCache::Get(int cache_slot) const {
    if (cache_slot < 0 || cache_slot >= size()) {
        throw std::range_error("VertexCache::Get: out of range");
    }
    const Entry &e = m_entries.at(cache_slot);
    return e.valid ? &e.vertex : nullptr;
}

Vtx *VertexCache::Get(int cache_slot) {
    if (cache_slot < 0 || cache_slot >= size()) {
        throw std::range_error("VertexCache::Get: out of range");
    }
    Entry &e = m_entries.at(cache_slot);
    return e.valid ? &e.vertex : nullptr;
}

int VertexCache::CachePos(std::array<int16_t, 3> pos) const {
    auto it = m_pos.find(pos);
    if (it == m_pos.end()) {
        return -1;
    }
    return it->second;
}

void VertexCache::Erase(int cache_slot) {
    if (cache_slot < 0 || cache_slot >= size()) {
        throw std::range_error("VertexCache::Erase: out of range");
    }
    EraseEntry(cache_slot, m_entries.at(cache_slot));
}

void VertexCache::Clear() {
    for (Entry &e : m_entries) {
        e.valid = false;
    }
    m_pos.clear();
}

void VertexCache::Set(int cache_slot, const Vtx &v) {
    if (cache_slot < 0 || cache_slot >= size()) {
        throw std::range_error("VertexCache::Set: out of range");
    }
    Entry &e = m_entries.at(cache_slot);
    EraseEntry(cache_slot, e);
    m_pos[v.pos] = cache_slot;
    e.valid = true;
    e.vertex = v;
}

void VertexCache::EraseEntry(int cache_slot, Entry &e) {
    if (e.valid) {
        auto it = m_pos.find(e.vertex.pos);
        if (it != m_pos.end() && it->second == cache_slot) {
            m_pos.erase(it);
        }
        e.valid = false;
    }
}

} // namespace gbi
} // namespace modelconvert
