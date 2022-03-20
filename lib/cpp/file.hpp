#include <algorithm>
#include <cstdio>

namespace util {

// Wrapper for std::FILE.
class File {
public:
    File() : m_file{nullptr} {}
    File(const File &) = delete;
    File(File &&other) : m_file{other.m_file} { other.m_file = nullptr; }
    explicit File(std::FILE *file) : m_file{file} {}
    ~File() {
        if (m_file != nullptr)
            std::fclose(m_file);
    }
    File &operator=(const File &) = delete;
    File &operator=(File &&other) {
        std::swap(m_file, other.m_file);
        return *this;
    }
    operator std::FILE *() { return m_file; }
    operator bool() { return m_file != nullptr; }
    bool operator!() { return m_file == nullptr; }

private:
    std::FILE *m_file;
};

} // namespace util
