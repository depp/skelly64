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

struct FHeader {
    static constexpr size_t Size = 12;

    uint32_t vertex_offset;
    uint32_t dl_offset;
    uint32_t animation_count;

    void Swap() {
        vertex_offset = BSwap32(vertex_offset);
        dl_offset = BSwap32(dl_offset);
        animation_count = BSwap32(animation_count);
    }
};

struct FAnimation {
    static constexpr size_t Size = 12;

    uint32_t duration;
    uint32_t frame_count;
    uint32_t frame_offset;

    void Swap() {
        duration = BSwap32(duration);
        frame_count = BSwap32(frame_count);
        frame_offset = BSwap32(frame_offset);
    }
};

struct FFrame {
    static constexpr size_t Size = 12;

    uint32_t time;
    uint32_t inv_dt;
    uint32_t vertex_offset;

    void Swap() {
        time = BSwap32(time);
        inv_dt = BSwap32(inv_dt);
        vertex_offset = BSwap32(vertex_offset);
    }
};

template <typename T>
void WriteData(std::vector<uint8_t> *out, uint32_t pos, const T &data) {
    static_assert(sizeof(T) == T::Size);
    T bedata{data};
    bedata.Swap();
    if (pos > out->size() || T::Size > out->size() - pos) {
        throw std::range_error("WriteData");
    }
    std::memcpy(out->data() + pos, &bedata, T::Size);
}

size_t FrameCount(const Model &model) {
    size_t n = 0;
    for (const Animation &anim : model.animation) {
        n += anim.frame.size();
    }
    return n;
}

} // namespace

std::vector<uint8_t> Model::Emit(const Config &cfg) const {
    (void)&cfg;

    // Size of position data for one frame.
    const size_t vertexcount = vertex.size();
    const size_t framedata_size = vertexcount * Vtx::Size;

    // Calculate model layout.
    const size_t magiclen = 16;

    const size_t headerpos = Align(magiclen);
    const size_t headerlen = FHeader::Size;
    size_t animpos = headerpos + headerlen;
    const size_t animlen = FAnimation::Size * animation.size();
    size_t framepos = animpos + animlen;
    const size_t framelen = FFrame::Size * FrameCount(*this);

    const size_t dlpos = Align(framepos + framelen);
    const size_t dllen = Gfx::Size * command.size();
    const size_t vertexpos = Align(dlpos + dllen);
    const size_t vertexlen = Vtx::Size * vertex.size();
    const size_t fdatapos = Align(vertexpos + vertexlen);
    const size_t fdatalen = framedata_size * frame.size();

    const size_t size = Align(fdatapos + fdatalen);

    std::vector<uint8_t> data(size, 0);

    // Emit magic.
    {
        const std::string_view magic{"Model"};
        std::copy(magic.begin(), magic.end(), data.begin());
    }

    // Emit header.
    {
        FHeader h{};
        h.vertex_offset = vertexpos - headerpos;
        h.dl_offset = dlpos - headerpos;
        h.animation_count = animation.size();
        WriteData(&data, headerpos, h);
    }

    // Emit animations.
    for (const Animation &anim : animation) {
        {
            FAnimation a{};
            a.duration = util::PutFloat32(anim.duration);
            a.frame_count = anim.frame.size();
            a.frame_offset = framepos - headerpos;
            WriteData(&data, animpos, a);
            animpos += a.Size;
        }
        for (size_t i = 0; i < anim.frame.size(); i++) {
            const AnimationFrame &frame = anim.frame.at(i);
            FFrame f{};
            f.time = frame.time;
            float next_time = i + 1 < anim.frame.size() ? anim.frame[i + 1].time
                                                        : anim.duration;
            float dt = next_time - frame.time;
            f.inv_dt = util::PutFloat32(dt < 1.0e-3f ? 0.0f : 1.0f / dt);
            f.vertex_offset =
                fdatapos + framedata_size * frame.index - headerpos;
            WriteData(&data, framepos, f);
            framepos += f.Size;
        }
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

    // Emit frame data.
    {
        uint8_t *ptr = data.data() + fdatapos;
        for (const FrameData &fdata : frame) {
            if (fdata.pos.size() != vertexcount) {
                throw std::runtime_error("bad frame data size");
            }
            for (size_t i = 0; i < vertexcount; i++) {
                const Vtx &attr = vertex.at(i);
                Vtx v{};
                v.pos = fdata.pos.at(i).pos;
                v.texcoord = attr.texcoord;
                v.color = attr.color;
                v.Write(ptr);
                ptr += Vtx::Size;
            }
        }
    }

    return data;
}

} // namespace gbi
} // namespace modelconvert
