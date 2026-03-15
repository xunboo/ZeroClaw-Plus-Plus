#pragma once

/// Auto-detection of available security features

#include "traits.hpp"
#include <memory>
#include <string>
#include <optional>
#include <vector>

namespace zeroclaw {
namespace security {

/// Sandbox backend configuration enum (mirrors Rust SandboxBackend)
enum class SandboxBackend {
    Auto,
    None,
    Landlock,
    Firejail,
    Bubblewrap,
    Docker
};

/// Sandbox configuration (mirrors Rust SandboxConfig)
struct SandboxConfig {
    std::optional<bool> enabled;
    SandboxBackend backend = SandboxBackend::Auto;
    std::vector<std::string> firejail_args;
};

/// Security configuration (mirrors Rust SecurityConfig)
struct SecurityConfig {
    SandboxConfig sandbox;
};

/// Create a sandbox based on auto-detection or explicit config
std::shared_ptr<Sandbox> create_sandbox(const SecurityConfig& config);

/// Auto-detect the best available sandbox
std::shared_ptr<Sandbox> detect_best_sandbox();

} // namespace security
} // namespace zeroclaw
