#pragma once

/// Identity module — agent identity configuration.

#include <string>
#include <optional>

namespace zeroclaw {

/// Agent identity configuration
struct Identity {
    std::string name = "ZeroClaw";
    std::string persona;
    std::optional<std::string> avatar_url;
    std::optional<std::string> custom_instructions;

    /// Build identity prompt section
    std::string build_prompt() const;
};

} // namespace zeroclaw
