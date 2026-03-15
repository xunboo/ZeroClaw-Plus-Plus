#include "audit.hpp"
#include <sstream>
#include <iomanip>
#include <random>
#include <chrono>

namespace zeroclaw {
namespace security {

// ── Helpers ──────────────────────────────────────────────────────

static std::string generate_uuid() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);

    auto r = [&]() { return dist(gen); };
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    uint32_t a = r(), b = r(), c = r(), d = r();
    oss << std::setw(8) << a << '-'
        << std::setw(4) << (b >> 16) << '-'
        << std::setw(4) << ((b & 0xFFFF) | 0x4000) << '-'
        << std::setw(4) << ((c >> 16) | 0x8000) << '-'
        << std::setw(4) << (c & 0xFFFF) << std::setw(8) << d;
    return oss.str();
}

static std::string now_iso8601() {
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm utc_tm{};
#ifdef _WIN32
    gmtime_s(&utc_tm, &tt);
#else
    gmtime_r(&tt, &utc_tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%S") << "Z";
    return oss.str();
}

// ── AuditEvent ───────────────────────────────────────────────────

AuditEvent AuditEvent::create(AuditEventType type) {
    AuditEvent event;
    event.timestamp = now_iso8601();
    event.event_id = generate_uuid();
    event.event_type = type;
    return event;
}

AuditEvent& AuditEvent::with_actor(const std::string& channel,
                                    const std::optional<std::string>& user_id,
                                    const std::optional<std::string>& username) {
    actor = Actor{channel, user_id, username};
    return *this;
}

AuditEvent& AuditEvent::with_action(const std::string& command, const std::string& risk_level,
                                     bool approved_val, bool allowed_val) {
    action = Action{command, risk_level, approved_val, allowed_val};
    return *this;
}

AuditEvent& AuditEvent::with_result(bool success, std::optional<int32_t> exit_code,
                                     uint64_t duration_ms, std::optional<std::string> error) {
    result = ExecutionResult{success, exit_code, duration_ms, error};
    return *this;
}

AuditEvent& AuditEvent::with_security(const std::optional<std::string>& sandbox_backend) {
    security.sandbox_backend = sandbox_backend;
    return *this;
}

nlohmann::json AuditEvent::to_json() const {
    nlohmann::json j;
    j["timestamp"] = timestamp;
    j["event_id"] = event_id;
    j["event_type"] = audit_event_type_to_string(event_type);

    if (actor) {
        nlohmann::json a;
        a["channel"] = actor->channel;
        if (actor->user_id) a["user_id"] = *actor->user_id;
        if (actor->username) a["username"] = *actor->username;
        j["actor"] = a;
    }

    if (action) {
        nlohmann::json a;
        if (action->command) a["command"] = *action->command;
        if (action->risk_level) a["risk_level"] = *action->risk_level;
        a["approved"] = action->approved;
        a["allowed"] = action->allowed;
        j["action"] = a;
    }

    if (result) {
        nlohmann::json r;
        r["success"] = result->success;
        if (result->exit_code) r["exit_code"] = *result->exit_code;
        if (result->duration_ms) r["duration_ms"] = *result->duration_ms;
        if (result->error) r["error"] = *result->error;
        j["result"] = r;
    }

    nlohmann::json s;
    s["policy_violation"] = security.policy_violation;
    if (security.rate_limit_remaining) s["rate_limit_remaining"] = *security.rate_limit_remaining;
    if (security.sandbox_backend) s["sandbox_backend"] = *security.sandbox_backend;
    j["security"] = s;

    return j;
}

// ── AuditLogger ──────────────────────────────────────────────────

AuditLogger::AuditLogger(const AuditConfig& config, const std::filesystem::path& zeroclaw_dir)
    : log_path_(zeroclaw_dir / config.log_path), config_(config) {}

bool AuditLogger::log(const AuditEvent& event) {
    if (!config_.enabled) return true;

    std::lock_guard<std::mutex> lock(mutex_);
    rotate_if_needed();

    auto json_str = event.to_json().dump();
    std::ofstream file(log_path_, std::ios::app);
    if (!file.is_open()) return false;

    file << json_str << "\n";
    file.flush();
    return true;
}

bool AuditLogger::log_command_event(const CommandExecutionLog& entry) {
    auto event = AuditEvent::create(AuditEventType::CommandExecution)
        .with_actor(entry.channel)
        .with_action(entry.command, entry.risk_level, entry.approved, entry.allowed)
        .with_result(entry.success, std::nullopt, entry.duration_ms);
    return log(event);
}

bool AuditLogger::log_command(const std::string& channel, const std::string& command,
                               const std::string& risk_level, bool approved, bool allowed,
                               bool success, uint64_t duration_ms) {
    return log_command_event(CommandExecutionLog{
        channel, command, risk_level, approved, allowed, success, duration_ms
    });
}

void AuditLogger::rotate_if_needed() {
    std::error_code ec;
    auto size = std::filesystem::file_size(log_path_, ec);
    if (ec) return;

    uint64_t size_mb = size / (1024 * 1024);
    if (size_mb >= config_.max_size_mb) {
        rotate();
    }
}

void AuditLogger::rotate() {
    auto path_str = log_path_.string();
    for (int i = 9; i >= 1; --i) {
        auto old_name = path_str + "." + std::to_string(i) + ".log";
        auto new_name = path_str + "." + std::to_string(i + 1) + ".log";
        std::error_code ec;
        std::filesystem::rename(old_name, new_name, ec);
    }

    auto rotated = path_str + ".1.log";
    std::error_code ec;
    std::filesystem::rename(log_path_, rotated, ec);
}

} // namespace security
} // namespace zeroclaw
