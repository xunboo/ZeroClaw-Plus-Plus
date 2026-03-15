#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <mutex>
#include <optional>
#include <chrono>
#include <functional>

#include "../security/policy.hpp"
#include "../config/config.hpp"

namespace zeroclaw {
namespace approval {

enum class ApprovalResponse {
    Yes,
    No,
    Always
};

std::string approval_response_to_string(ApprovalResponse resp);
ApprovalResponse approval_response_from_string(const std::string& s);

struct ApprovalRequest {
    std::string tool_name;
    std::string arguments_json;
};

struct ApprovalLogEntry {
    std::string timestamp;
    std::string tool_name;
    std::string arguments_summary;
    ApprovalResponse decision;
    std::string channel;
};

class ApprovalManager {
public:
    using PromptCallback = std::function<ApprovalResponse(const ApprovalRequest&)>;

private:
    std::unordered_set<std::string> auto_approve_;
    std::unordered_set<std::string> always_ask_;
    security::AutonomyLevel autonomy_level_;
    mutable std::mutex mutex_;
    std::unordered_set<std::string> session_allowlist_;
    std::vector<ApprovalLogEntry> audit_log_;
    PromptCallback prompt_callback_;

public:
    ApprovalManager() = default;
    
    explicit ApprovalManager(const config::AutonomyConfig& config);
    
    void set_prompt_callback(PromptCallback callback);
    
    bool needs_approval(const std::string& tool_name) const;
    
    void record_decision(
        const std::string& tool_name,
        const std::string& args_json,
        ApprovalResponse decision,
        const std::string& channel
    );
    
    std::vector<ApprovalLogEntry> audit_log() const;
    
    std::unordered_set<std::string> session_allowlist() const;
    
    ApprovalResponse prompt_cli(const ApprovalRequest& request) const;

private:
    static std::string get_timestamp();
};

std::string summarize_args(const std::string& args_json);
std::string truncate_for_summary(const std::string& input, size_t max_chars);
ApprovalResponse prompt_cli_interactive(const ApprovalRequest& request);

}
}
