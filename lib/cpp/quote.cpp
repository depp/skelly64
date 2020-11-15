#include "tools/util/quote.hpp"

namespace util {

namespace {

const char HEX_DIGIT[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                            '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
}

std::string Quote(std::string_view str) {
    std::string out;
    out.push_back('"');
    for (const char c : str) {
        if (32 <= c && c <= 126) {
            if (c == '"' || c == '\\') {
                out.push_back('\\');
            }
            out.push_back(c);
        } else {
            out.push_back('\\');
            switch (c) {
            case '\n':
                out.push_back('n');
                break;
            case '\r':
                out.push_back('r');
                break;
            case '\t':
                out.push_back('t');
                break;
            default:
                out.push_back('x');
                out.push_back(HEX_DIGIT[c >> 4]);
                out.push_back(HEX_DIGIT[c & 15]);
                break;
            }
        }
    }
    out.push_back('"');
    return out;
}

} // namespace util
