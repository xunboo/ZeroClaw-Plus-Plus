#pragma once

/// Landlock sandbox (Linux kernel 5.13+ LSM)
///
/// Landlock provides unprivileged sandboxing through the Linux kernel.
/// This is a cross-platform stub; real Landlock support only works on Linux.

#include "traits.hpp"
#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <optional>

namespace zeroclaw {
namespace security {

/// Landlock sandbox backend for Linux
class LandlockSandbox : public Sandbox {
public:
    LandlockSandbox() = default;
    explicit LandlockSandbox(const std::optional<std::filesystem::path>& workspace_dir)
        : workspace_dir_(workspace_dir) {}

    /// Create a new Landlock sandbox
    static std::unique_ptr<LandlockSandbox> create();

    /// Create with a specific workspace directory
    static std::unique_ptr<LandlockSandbox> create_with_workspace(
        const std::optional<std::filesystem::path>& workspace_dir);

    /// Probe if Landlock is available
    static std::unique_ptr<LandlockSandbox> probe();

    // Sandbox interface
    bool wrap_command(std::string& program, std::vector<std::string>& args) override;
    bool is_available() const override;
    std::string name() const override { return "landlock"; }
    std::string description() const override;

private:
    std::optional<std::filesystem::path> workspace_dir_;
};

} // namespace security
} // namespace zeroclaw
