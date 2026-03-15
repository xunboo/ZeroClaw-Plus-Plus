#pragma once

/// Audit logging for security events

#include <string>
#include <vector>
#include <mutex>
#include <optional>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <chrono>
#include "nlohmann/json.hpp"

namespace zeroclaw {
namespace security {

/// Audit event types
enum class AuditEventType {
    CommandExecution,
    FileAccess,
    ConfigChange,
    AuthSuccess,
    AuthFailure,
    PolicyViolation,
    SecurityEvent
};

inline std::string audit_event_type_to_string(AuditEventType t) {
    switch (t) {
        case AuditEventType::CommandExecution: return "command_execution";
        case AuditEventType::FileAccess: return "file_access";
        case AuditEventType::ConfigChange: return "config_change";
        case AuditEventType::AuthSuccess: return "auth_success";
        case AuditEventType::AuthFailure: return "auth_failure";
        case AuditEventType::PolicyViolation: return "policy_violation";
        case AuditEventType::SecurityEvent: return "security_event";
    }
    return "unknown";
}

/// Actor information (who performed the action)
struct Actor {
    std::string channel;
    std::optional<std::string> user_id;
    std::optional<std::string> username;
};

/// Action information (what was done)
struct Action {
    std::optional<std::string> command;
    std::optional<std::string> risk_level;
    bool approved = false;
    bool allowed = false;
};

/// Execution result
struct ExecutionResult {
    bool success = false;
    std::optional<int32_t> exit_code;
    std::optional<uint64_t> duration_ms;
    std::optional<std::string> error;
};

/// Security context
struct SecurityContext {
    bool policy_violation = false;
    std::optional<uint32_t> rate_limit_remaining;
    std::optional<std::string> sandbox_backend;
};

/// Complete audit event
struct AuditEvent {
    std::string timestamp;
    std::string event_id;
    AuditEventType event_type = AuditEventType::CommandExecution;
    std::optional<Actor> actor;
    std::optional<Action> action;
    std::optional<ExecutionResult> result;
    SecurityContext security;

    /// Create a new audit event with a unique ID and current timestamp
    static AuditEvent create(AuditEventType type);

    /// Builder methods (return *this for chaining)
    AuditEvent& with_actor(const std::string& channel,
                           const std::optional<std::string>& user_id = std::nullopt,
                           const std::optional<std::string>& username = std::nullopt);
    AuditEvent& with_action(const std::string& command, const std::string& risk_level,
                            bool approved, bool allowed);
    AuditEvent& with_result(bool success, std::optional<int32_t> exit_code,
                            uint64_t duration_ms, std::optional<std::string> error = std::nullopt);
    AuditEvent& with_security(const std::optional<std::string>& sandbox_backend);

    /// Serialize to JSON
    nlohmann::json to_json() const;
};

/// Audit config (mirrors Rust AuditConfig)
struct AuditConfig {
    bool enabled = false;
    std::string log_path = "audit.log";
    uint32_t max_size_mb = 10;
};

/// Structured command execution details for audit logging
struct CommandExecutionLog {
    std::string channel;
    std::string command;
    std::string risk_level;
    bool approved = false;
    bool allowed = false;
    bool success = false;
    uint64_t duration_ms = 0;
};

/// Audit logger
class AuditLogger {
public:
    AuditLogger(const AuditConfig& config, const std::filesystem::path& zeroclaw_dir);

    /// Log an event
    bool log(const AuditEvent& event);

    /// Log a command execution event
    bool log_command_event(const CommandExecutionLog& entry);

    /// Backward-compatible helper
    bool log_command(const std::string& channel, const std::string& command,
                     const std::string& risk_level, bool approved, bool allowed,
                     bool success, uint64_t duration_ms);

private:
    void rotate_if_needed();
    void rotate();

    std::filesystem::path log_path_;
    AuditConfig config_;
    mutable std::mutex mutex_;
    std::vector<AuditEvent> buffer_;
};

} // namespace security
} // namespace zeroclaw
