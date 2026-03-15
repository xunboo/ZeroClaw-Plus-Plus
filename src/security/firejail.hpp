#pragma once

/// Firejail sandbox (Linux user-space sandboxing)

#include "traits.hpp"
#include <string>
#include <vector>
#include <memory>

namespace zeroclaw {
namespace security {

/// Firejail sandbox backend for Linux
class FirejailSandbox : public Sandbox {
public:
    FirejailSandbox() = default;

    /// Create (checks if firejail is installed)
    static std::unique_ptr<FirejailSandbox> create();

    /// Probe if firejail is available
    static std::unique_ptr<FirejailSandbox> probe();

    /// Check if firejail is installed
    static bool is_installed();

    // Sandbox interface
    bool wrap_command(std::string& program, std::vector<std::string>& args) override;
    bool is_available() const override;
    std::string name() const override { return "firejail"; }
    std::string description() const override {
        return "Linux user-space sandbox (requires firejail to be installed)";
    }
};

} // namespace security
} // namespace zeroclaw
