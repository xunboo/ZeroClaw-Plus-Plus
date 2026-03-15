#pragma once

/// Sandbox trait for pluggable OS-level isolation.
///
/// This module defines the Sandbox interface, which abstracts OS-level process
/// isolation backends. Implementations wrap shell commands with platform-specific
/// sandboxing to limit the blast radius of tool execution.

#include <string>
#include <vector>
#include <memory>

namespace zeroclaw {
namespace security {

/// Sandbox backend for OS-level process isolation.
///
/// Implement this interface to add a new sandboxing strategy. The runtime queries
/// is_available() at startup to select the best backend for the current platform,
/// then calls wrap_command() before every shell execution.
class Sandbox {
public:
    virtual ~Sandbox() = default;

    /// Wrap a command with sandbox protection.
    /// Mutates the program/args to apply isolation constraints.
    /// Returns true on success, false on failure.
    virtual bool wrap_command(std::string& program, std::vector<std::string>& args) = 0;

    /// Check if this sandbox backend is available on the current platform.
    virtual bool is_available() const = 0;

    /// Return the human-readable name of this sandbox backend.
    virtual std::string name() const = 0;

    /// Return a brief description of the isolation guarantees.
    virtual std::string description() const = 0;
};

/// No-op sandbox that provides no additional OS-level isolation.
/// Always reports itself as available. Use as fallback.
class NoopSandbox : public Sandbox {
public:
    bool wrap_command(std::string& /*program*/, std::vector<std::string>& /*args*/) override {
        return true; // Pass through unchanged
    }

    bool is_available() const override { return true; }
    std::string name() const override { return "none"; }
    std::string description() const override {
        return "No sandboxing (application-layer security only)";
    }
};

} // namespace security
} // namespace zeroclaw
