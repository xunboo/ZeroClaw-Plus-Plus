#pragma once

/// Shell tool — sandboxed shell command execution.

#include <string>
#include <vector>
#include <memory>
#include "traits.hpp"
#include "nlohmann/json.hpp"

namespace zeroclaw {

// Forward declarations
namespace security { class SecurityPolicy; }

namespace tools {

/// Safe environment variables to pass through to shell commands
static const std::vector<std::string> SAFE_ENV_VARS = {
    "PATH", "HOME", "TERM", "LANG", "LC_ALL", "LC_CTYPE", "USER", "SHELL", "TMPDIR"
};

/// Default command timeout in seconds
static constexpr int DEFAULT_SHELL_TIMEOUT_SECS = 120;

/// Maximum output length in bytes
static constexpr size_t MAX_SHELL_OUTPUT_BYTES = 1 * 1024 * 1024;

/// Check if a string is a valid environment variable name
bool is_valid_env_var_name(const std::string& name);

/// Collect allowed environment variables based on security policy
std::vector<std::string> collect_allowed_shell_env_vars(
    const security::SecurityPolicy& security);

/// Shell command execution tool with sandboxing
class ShellTool : public Tool {
public:
    explicit ShellTool(std::shared_ptr<security::SecurityPolicy> security);

    std::string name() const override { return "shell"; }

    std::string description() const override {
        return "Execute a shell command in a sandboxed environment. "
               "The command runs within the workspace directory.";
    }

    nlohmann::json parameters_schema() const override {
        return {
            {"type", "object"},
            {"properties", {
                {"command", {
                    {"type", "string"},
                    {"description", "The shell command to execute"}
                }},
                {"timeout", {
                    {"type", "integer"},
                    {"description", "Timeout in seconds (default: 120)"}
                }},
                {"working_dir", {
                    {"type", "string"},
                    {"description", "Working directory relative to workspace (default: workspace root)"}
                }}
            }},
            {"required", nlohmann::json::array({"command"})}
        };
    }

    ToolResult execute(const nlohmann::json& args) override;

private:
    std::shared_ptr<security::SecurityPolicy> security_;
};

} // namespace tools
} // namespace zeroclaw
