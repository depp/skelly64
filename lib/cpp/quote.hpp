#pragma once

#include <string>
#include <string_view>

namespace util {

// Quote a string with double quotes and escaepe the contents.
std::string Quote(std::string_view str);

} // namespace util
