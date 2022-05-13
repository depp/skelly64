#include <cstdlib>
#include <utility>

struct vadpcm_codebook;

namespace vadpcm {

// Wrapper for vadpcm_codebook objects.
class Codebook {
public:
    Codebook() : m_ptr{nullptr} {}
    explicit Codebook(vadpcm_codebook *ptr) : m_ptr{ptr} {}
    Codebook(const Codebook &) = delete;
    Codebook(Codebook &&other) : m_ptr{other.m_ptr} { other.m_ptr = nullptr; }
    Codebook &operator=(const Codebook &) = delete;
    Codebook &operator=(Codebook &&other) {
        std::swap(m_ptr, other.m_ptr);
        return *this;
    }
    ~Codebook() { std::free(m_ptr); }

    operator vadpcm_codebook *() { return m_ptr; }
    operator bool() const { return m_ptr != nullptr; }
    bool operator!() const { return m_ptr == nullptr; }

private:
    vadpcm_codebook *m_ptr;
};

} // namespace vadpcm
