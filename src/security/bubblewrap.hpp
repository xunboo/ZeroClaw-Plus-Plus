#pragma once

/// Bubblewrap sandbox (user namespaces for Linux/macOS)

#include "traits.hpp"
#include <string>
#include <vector>
#include <memory>

namespace zeroclaw {
namespace security {

/// Bubblewrap sandbox backend
class BubblewrapSandbox : public Sandbox {
public:
    BubblewrapSandbox() = default;

    /// Create (checks if bwrap is installed)
    static std::unique_ptr<BubblewrapSandbox> create();

    /// Probe if Bubblewrap is available
    static std::unique_ptr<BubblewrapSandbox> probe();

    /// Check if bwrap is installed
    static bool is_installed();

    // Sandbox interface
    bool wrap_command(std::string& program, std::vector<std::string>& args) override;
    bool is_available() const override;
    std::string name() const override { return "bubblewrap"; }
    std::string description() const override {
        return "User namespace sandbox (requires bwrap)";
    }
};

} // namespace security
} // namespace zeroclaw
