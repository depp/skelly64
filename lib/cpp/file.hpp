#pragma once

#include <algorithm>
#include <cstdio>
#include <string>

namespace util {

// Wrapper for std::FILE.
class File {
public:
    File() : m_file{nullptr} {}
    File(const File &) = delete;
    File(File &&other) : m_file{other.m_file}, m_name{std::move(other.m_name)} {
        other.m_file = nullptr;
    }
    explicit File(std::FILE *file) : m_file{file} {}
    ~File() {
        if (m_file != nullptr) {
            std::fclose(m_file);
        }
    }
    File &operator=(const File &) = delete;
    File &operator=(File &&other) {
        using std::swap;
        swap(m_file, other.m_file);
        swap(m_name, other.m_name);
        return *this;
    }
    operator std::FILE *() { return m_file; }
    operator bool() { return m_file != nullptr; }
    bool operator!() { return m_file == nullptr; }

    // Set the file handle and its name.
    void Set(std::FILE *file, std::string_view name);

    // Close the file. Raises an error if data is not flushed.
    void Close();

    // Return the name of the file, used to open it.
    std::string_view name() const { return m_name; }

    // Get the file handle.
    std::FILE *file() { return m_file; }

    // Seek to the given offset.
    void Seek(int64_t offset);

private:
    std::FILE *m_file;
    std::string m_name;
};

// A wrapper for input files.
class InputFile : private File {
public:
    void Open(const std::string &name);

    // Read the given number of bytes, and throw an error on failure.
    void ReadExact(void *ptr, size_t amt);

    using File::name;
    using File::Seek;
};

// A wrapper for output files.
class OutputFile : private File {
public:
    void Create(const std::string &name);

    // Commit all data to disk.
    void Commit();

    using File::name;
    using File::Seek;

    void Write(const void *ptr, size_t size);
};

} // namespace util
