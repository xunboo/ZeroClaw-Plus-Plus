#pragma once

#include "traits.hpp"
#include "backend.hpp"
#include <string>
#include <algorithm>
#include <cctype>

namespace zeroclaw::memory {

inline MemoryCategory parse_category(const std::string& s) {
    std::string lower = s;
    size_t start = lower.find_first_not_of(" \t");
    size_t end = lower.find_last_not_of(" \t");
    if (start != std::string::npos && end != std::string::npos) {
        lower = lower.substr(start, end - start + 1);
    }
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    
    if (lower == "core") return MemoryCategory(MemoryCategory::Kind::Core);
    if (lower == "daily") return MemoryCategory(MemoryCategory::Kind::Daily);
    if (lower == "conversation") return MemoryCategory(MemoryCategory::Kind::Conversation);
    return MemoryCategory(s);
}

inline std::string truncate_content(const std::string& s, size_t max_len) {
    size_t nl = s.find('\n');
    std::string line = (nl != std::string::npos) ? s.substr(0, nl) : s;
    if (line.size() <= max_len) return line;
    return line.substr(0, max_len - 3) + "...";
}

}
