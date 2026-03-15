#pragma once

/// Emergency stop (estop) manager for security kill switches.

#include "domain_matcher.hpp"
#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <stdexcept>

namespace zeroclaw {
namespace security {

// Forward declaration
class OtpValidator;

/// Estop severity levels
enum class EstopLevel {
    KillAll,
    NetworkKill,
};

/// Estop level with associated data
struct EstopAction {
    enum class Type { KillAll, NetworkKill, DomainBlock, ToolFreeze };
    Type type;
    std::vector<std::string> items; // domains for DomainBlock, tools for ToolFreeze
};

/// Resume selector
struct ResumeSelector {
    enum class Type { KillAll, Network, Domains, Tools };
    Type type;
    std::vector<std::string> items; // domains or tools
};

/// Estop config (mirrors Rust EstopConfig)
struct EstopConfig {
    bool enabled = false;
    std::string state_file = "estop-state.json";
    bool require_otp_to_resume = false;
};

/// Persistent estop state
struct EstopState {
    bool kill_all = false;
    bool network_kill = false;
    std::vector<std::string> blocked_domains;
    std::vector<std::string> frozen_tools;
    std::optional<std::string> updated_at;

    /// Create fail-closed state
    static EstopState fail_closed();

    /// Check if any estop is engaged
    bool is_engaged() const;

    /// Normalize (sort + dedup) internal vectors
    void normalize();
};

/// Manages estop state, persisting to a JSON file.
class EstopManager {
public:
    /// Load from config and state file
    static EstopManager load(const EstopConfig& config, const std::filesystem::path& config_dir);

    /// Path to the state file
    const std::filesystem::path& state_path() const { return state_path_; }

    /// Current state
    EstopState status() const { return state_; }

    /// Engage an estop level
    void engage(const EstopAction& action);

    /// Resume (lift) an estop level
    void resume(const ResumeSelector& selector,
                const std::optional<std::string>& otp_code = std::nullopt,
                const OtpValidator* otp_validator = nullptr);

private:
    EstopManager() = default;

    void ensure_resume_is_authorized(const std::optional<std::string>& otp_code,
                                      const OtpValidator* otp_validator);
    void persist_state();

    EstopConfig config_;
    std::filesystem::path state_path_;
    EstopState state_;
};

/// Resolve the state file path (expand ~ and handle relative paths)
std::filesystem::path resolve_state_file_path(const std::filesystem::path& config_dir,
                                               const std::string& state_file);

/// Normalize a tool name (lowercase, validate characters)
std::string normalize_tool_name(const std::string& raw);

} // namespace security
} // namespace zeroclaw
