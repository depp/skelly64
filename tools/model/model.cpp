#include "tools/modelconvert/model.hpp"

#include "tools/modelconvert/config.hpp"
#include "tools/util/bswap.hpp"

#include <cstring>
#include <string_view>

namespace modelconvert {
namespace gbi {

using util::BSwap16;
using util::BSwap32;

namespace {

size_t Align(size_t x) {
    return (x + 15) & ~static_cast<size_t>(15);
}

struct Header {
    static constexpr size_t Size = 8;

    uint32_t vertex_offset;
    uint32_t dl_offset;

    void Swap() {
        vertex_offset = BSwap32(vertex_offset);
        dl_offset = BSwap32(dl_offset);
    }

    void Write(uint8_t *ptr) const {
        static_assert(sizeof(Header) == Size);
        Header h{*this};
        h.Swap();
        std::memcpy(ptr, &h, Size);
    }
};

} // namespace

std::vector<uint8_t> Model::Emit(const Config &cfg) const {
    (void)&cfg;

    const size_t magiclen = 16;
    const size_t headerlen = Header::Size;
    const size_t dllen = Gfx::Size * command.size();
    const size_t vertexlen = Vtx::Size * vertex.size();

    const size_t headerpos = Align(magiclen);
    const size_t dlpos = Align(headerpos + headerlen);
    const size_t vertexpos = Align(dlpos + dllen);
    const size_t size = Align(vertexpos + vertexlen);

    std::vector<uint8_t> data(size, 0);

    // Emit magic.
    {
        const std::string_view magic{"Model"};
        std::copy(magic.begin(), magic.end(), data.begin());
    }

    // Emit header.
    {
        Header h{};
        h.vertex_offset = vertexpos - headerpos;
        h.dl_offset = dlpos - headerpos;
        h.Write(data.data() + headerpos);
    }

    // Emit the display list.
    {
        uint8_t *ptr = data.data() + dlpos;
        for (const Gfx &g : command) {
            g.Write(ptr);
            ptr += Gfx::Size;
        }
    }

    // Emit the vertex data.
    {
        uint8_t *ptr = data.data() + vertexpos;
        for (const Vtx &v : vertex) {
            v.Write(ptr);
            ptr += Vtx::Size;
        }
    }

    return data;
}

} // namespace gbi
} // namespace modelconvert
