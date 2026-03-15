#include "util.hpp"
#include <algorithm>

namespace zeroclaw {
namespace util {

std::string truncate_with_ellipsis(const std::string& s, size_t max_chars) {
    if (s.empty() || max_chars == 0) {
        return s.empty() ? "" : "...";
    }
    
    size_t char_count = 0;
    size_t byte_idx = 0;
    
    while (byte_idx < s.size() && char_count < max_chars) {
        unsigned char c = static_cast<unsigned char>(s[byte_idx]);
        size_t char_len = 1;
        
        if ((c & 0x80) == 0) {
            char_len = 1;
        } else if ((c & 0xE0) == 0xC0) {
            char_len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            char_len = 3;
        } else if ((c & 0xF8) == 0xF0) {
            char_len = 4;
        }
        
        byte_idx += char_len;
        char_count++;
    }
    
    if (byte_idx >= s.size()) {
        return s;
    }
    
    std::string truncated = s.substr(0, byte_idx);
    
    size_t end = truncated.find_last_not_of(" \t\n\r\f\v");
    if (end != std::string::npos) {
        truncated = truncated.substr(0, end + 1);
    }
    
    return truncated + "...";
}

}
}
