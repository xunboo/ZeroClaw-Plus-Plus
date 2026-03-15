#pragma once

#include "traits.hpp"
#include <memory>
#include <stdexcept>
#include <regex>

namespace zeroclaw::memory {

inline void validate_identifier(const std::string& value, const std::string& field_name) {
    if (value.empty()) {
        throw MemoryError(field_name + " must not be empty");
    }
    
    char first = value[0];
    if (!std::isalpha(static_cast<unsigned char>(first)) && first != '_') {
        throw MemoryError(field_name + " must start with an ASCII letter or underscore; got '" + value + "'");
    }
    
    for (char ch : value) {
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') {
            throw MemoryError(field_name + " can only contain ASCII letters, numbers, and underscores; got '" + value + "'");
        }
    }
}

inline std::string quote_identifier(const std::string& value) {
    return "\"" + value + "\"";
}

}
