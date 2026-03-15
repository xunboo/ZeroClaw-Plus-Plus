#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <functional>
#include <filesystem>

namespace zeroclaw {
namespace security {

// ── Enums ────────────────────────────────────────────────────────

/// How much autonomy the agent has
enum class AutonomyLevel {
    ReadOnly,   // Can observe but not act
    Supervised, // Acts but requires approval for risky operations
    Full        // Autonomous execution within policy bounds
};

inline AutonomyLevel autonomy_level_from_string(const std::string& s) {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (lower == "readonly" || lower == "read_only") return AutonomyLevel::ReadOnly;
    if (lower == "full") return AutonomyLevel::Full;
    return AutonomyLevel::Supervised;
}

inline std::string autonomy_level_to_string(AutonomyLevel level) {
    switch (level) {
        case AutonomyLevel::ReadOnly: return "readonly";
        case AutonomyLevel::Supervised: return "supervised";
        case AutonomyLevel::Full: return "full";
        default: return "supervised";
    }
}

/// Risk score for shell command execution
enum class CommandRiskLevel {
    Low,
    Medium,
    High
};

/// Classifies whether a tool operation is read-only or side-effecting
enum class ToolOperation {
    Read,
    Act
};

// ── ActionTracker ────────────────────────────────────────────────

/// Sliding-window action tracker for rate limiting.
class ActionTracker {
public:
    ActionTracker() = default;
    ActionTracker(const ActionTracker& other) {
        std::lock_guard<std::mutex> lock(other.mutex_);
        actions_ = other.actions_;
    }
    ActionTracker& operator=(const ActionTracker& other) {
        if (this != &other) {
            std::lock_guard<std::mutex> lock1(mutex_);
            std::lock_guard<std::mutex> lock2(other.mutex_);
            actions_ = other.actions_;
        }
        return *this;
    }

    /// Record an action and return the current count within the window.
    size_t record() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto cutoff = std::chrono::steady_clock::now() - std::chrono::hours(1);
        actions_.erase(
            std::remove_if(actions_.begin(), actions_.end(),
                           [&cutoff](const auto& t) { return t <= cutoff; }),
            actions_.end());
        actions_.push_back(std::chrono::steady_clock::now());
        return actions_.size();
    }

    /// Count of actions in the current window without recording.
    size_t count() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto cutoff = std::chrono::steady_clock::now() - std::chrono::hours(1);
        actions_.erase(
            std::remove_if(actions_.begin(), actions_.end(),
                           [&cutoff](const auto& t) { return t <= cutoff; }),
            actions_.end());
        return actions_.size();
    }

private:
    mutable std::mutex mutex_;
    std::vector<std::chrono::steady_clock::time_point> actions_;
};

// ── Shell parsing helpers (declarations) ─────────────────────────

enum class QuoteState { None, Single, Double };

std::string skip_env_assignments(const std::string& s);
std::vector<std::string> split_unquoted_segments(const std::string& command);
bool contains_unquoted_single_ampersand(const std::string& command);
bool contains_unquoted_char(const std::string& command, char target);
bool contains_unquoted_shell_variable_expansion(const std::string& command);
std::string strip_wrapping_quotes(const std::string& token);
bool looks_like_path(const std::string& candidate);
std::string attached_short_option_value(const std::string& token);
std::string redirection_target(const std::string& token);

// Path helpers
std::filesystem::path home_dir();
std::filesystem::path expand_user_path(const std::string& path);

// ── SecurityPolicy ───────────────────────────────────────────────

/// Security policy enforced on all tool executions
class SecurityPolicy {
public:
    AutonomyLevel autonomy = AutonomyLevel::Supervised;
    std::filesystem::path workspace_dir = ".";
    bool workspace_only = true;
    std::vector<std::string> allowed_commands;
    std::vector<std::string> forbidden_paths;
    std::vector<std::filesystem::path> allowed_roots;
    uint32_t max_actions_per_hour = 20;
    uint32_t max_cost_per_day_cents = 500;
    bool require_approval_for_medium_risk = true;
    bool block_high_risk_commands = true;
    std::vector<std::string> shell_env_passthrough;
    ActionTracker tracker;

    /// Create with default values
    SecurityPolicy();

    /// Classify command risk. Any high-risk segment marks the whole command high.
    CommandRiskLevel command_risk_level(const std::string& command) const;

    /// Validate full command execution policy (allowlist + risk gate).
    /// Returns risk level on success, error message on failure.
    struct CommandValidationResult {
        bool allowed = false;
        CommandRiskLevel risk = CommandRiskLevel::Low;
        std::string error;
    };
    CommandValidationResult validate_command_execution(const std::string& command, bool approved) const;

    /// Check if a shell command is allowed by the allowlist.
    bool is_command_allowed(const std::string& command) const;

    /// Check for dangerous arguments that allow sub-command execution.
    bool is_args_safe(const std::string& base, const std::vector<std::string>& args) const;

    /// Return the first path-like argument blocked by path policy.
    std::string forbidden_path_argument(const std::string& command) const;

    /// Check if a file path is allowed (no path traversal, within workspace)
    bool is_path_allowed(const std::string& path) const;

    /// Validate that a resolved path is inside the workspace or an allowed root.
    bool is_resolved_path_allowed(const std::filesystem::path& resolved) const;

    /// Generate a violation message for a resolved path.
    std::string resolved_path_violation_message(const std::filesystem::path& resolved) const;

    /// Check if autonomy level permits any action at all
    bool can_act() const;

    /// Enforce policy for a tool operation.
    struct ToolOperationResult {
        bool allowed = false;
        std::string error;
    };
    ToolOperationResult enforce_tool_operation(ToolOperation operation, const std::string& operation_name) const;

    /// Record an action and check if the rate limit has been exceeded.
    bool record_action() const;

    /// Check if the rate limit would be exceeded without recording.
    bool is_rate_limited() const;
};

/// Redact sensitive values for safe logging. Shows first 4 chars + "***" suffix.
inline std::string redact(const std::string& value) {
    if (value.length() <= 4) {
        return "***";
    }
    return value.substr(0, 4) + "***";
}

} // namespace security
} // namespace zeroclaw
