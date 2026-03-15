#include "approval.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace zeroclaw {
namespace approval {

std::string approval_response_to_string(ApprovalResponse resp) {
    switch (resp) {
        case ApprovalResponse::Yes: return "yes";
        case ApprovalResponse::No: return "no";
        case ApprovalResponse::Always: return "always";
        default: return "no";
    }
}

ApprovalResponse approval_response_from_string(const std::string& s) {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    
    if (lower == "yes" || lower == "y") return ApprovalResponse::Yes;
    if (lower == "always" || lower == "a") return ApprovalResponse::Always;
    return ApprovalResponse::No;
}

ApprovalManager::ApprovalManager(const config::AutonomyConfig& config)
    : auto_approve_(config.auto_approve.begin(), config.auto_approve.end())
    , always_ask_(config.always_ask.begin(), config.always_ask.end())
    , autonomy_level_(security::autonomy_level_from_string(config.level))
    , prompt_callback_(prompt_cli_interactive)
{
}

void ApprovalManager::set_prompt_callback(PromptCallback callback) {
    prompt_callback_ = std::move(callback);
}

bool ApprovalManager::needs_approval(const std::string& tool_name) const {
    if (autonomy_level_ == security::AutonomyLevel::Full) {
        return false;
    }
    
    if (autonomy_level_ == security::AutonomyLevel::ReadOnly) {
        return false;
    }
    
    if (always_ask_.count(tool_name) > 0) {
        return true;
    }
    
    if (auto_approve_.count(tool_name) > 0) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    if (session_allowlist_.count(tool_name) > 0) {
        return false;
    }
    
    return true;
}

void ApprovalManager::record_decision(
    const std::string& tool_name,
    const std::string& args_json,
    ApprovalResponse decision,
    const std::string& channel
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (decision == ApprovalResponse::Always) {
        session_allowlist_.insert(tool_name);
    }
    
    std::string summary = summarize_args(args_json);
    ApprovalLogEntry entry{
        get_timestamp(),
        tool_name,
        summary,
        decision,
        channel
    };
    audit_log_.push_back(std::move(entry));
}

std::vector<ApprovalLogEntry> ApprovalManager::audit_log() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return audit_log_;
}

std::unordered_set<std::string> ApprovalManager::session_allowlist() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return session_allowlist_;
}

ApprovalResponse ApprovalManager::prompt_cli(const ApprovalRequest& request) const {
    if (prompt_callback_) {
        return prompt_callback_(request);
    }
    return ApprovalResponse::No;
}

std::string ApprovalManager::get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm_buf;
#ifdef _WIN32
    gmtime_s(&tm_buf, &now_time_t);
#else
    gmtime_r(&now_time_t, &tm_buf);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << now_ms.count() << 'Z';
    return oss.str();
}

std::string truncate_for_summary(const std::string& input, size_t max_chars) {
    if (input.size() <= max_chars) {
        return input;
    }
    
    size_t char_count = 0;
    size_t byte_pos = 0;
    
    while (byte_pos < input.size() && char_count < max_chars) {
        unsigned char c = static_cast<unsigned char>(input[byte_pos]);
        if ((c & 0x80) == 0) {
            byte_pos += 1;
        } else if ((c & 0xE0) == 0xC0) {
            byte_pos += 2;
        } else if ((c & 0xF0) == 0xE0) {
            byte_pos += 3;
        } else if ((c & 0xF8) == 0xF0) {
            byte_pos += 4;
        } else {
            byte_pos += 1;
        }
        char_count++;
    }
    
    std::string truncated = input.substr(0, byte_pos);
    return truncated + "...";
}

std::string summarize_args(const std::string& args_json) {
    if (args_json.empty() || args_json == "null") {
        return "";
    }
    
    if (args_json.front() != '{') {
        return truncate_for_summary(args_json, 120);
    }
    
    std::string result;
    std::string content = args_json.substr(1, args_json.size() - 2);
    
    size_t pos = 0;
    bool first = true;
    int brace_depth = 0;
    int bracket_depth = 0;
    bool in_string = false;
    bool escaped = false;
    std::string current_key;
    std::string current_value;
    bool parsing_key = true;
    bool found_colon = false;
    
    for (size_t i = 0; i < content.size(); ++i) {
        char c = content[i];
        
        if (escaped) {
            escaped = false;
            if (parsing_key) current_key += c;
            else current_value += c;
            continue;
        }
        
        if (c == '\\' && in_string) {
            escaped = true;
            if (parsing_key) current_key += c;
            else current_value += c;
            continue;
        }
        
        if (c == '"' && brace_depth == 0 && bracket_depth == 0) {
            in_string = !in_string;
            continue;
        }
        
        if (!in_string) {
            if (c == '{') brace_depth++;
            else if (c == '}') brace_depth--;
            else if (c == '[') bracket_depth++;
            else if (c == ']') bracket_depth--;
            else if (c == ':' && brace_depth == 0 && bracket_depth == 0 && parsing_key) {
                parsing_key = false;
                found_colon = true;
                continue;
            }
            else if (c == ',' && brace_depth == 0 && bracket_depth == 0) {
                if (!current_key.empty()) {
                    if (!first) result += ", ";
                    first = false;
                    
                    std::string key = current_key;
                    if (key.size() >= 2 && key.front() == '"' && key.back() == '"') {
                        key = key.substr(1, key.size() - 2);
                    }
                    
                    std::string val = current_value;
                    val.erase(0, val.find_first_not_of(" \t\n\r"));
                    val.erase(val.find_last_not_of(" \t\n\r") + 1);
                    
                    if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
                        val = val.substr(1, val.size() - 2);
                    }
                    
                    result += key + ": " + truncate_for_summary(val, 80);
                }
                current_key.clear();
                current_value.clear();
                parsing_key = true;
                found_colon = false;
                continue;
            }
        }
        
        if (parsing_key) {
            current_key += c;
        } else {
            current_value += c;
        }
    }
    
    if (!current_key.empty()) {
        if (!first) result += ", ";
        
        std::string key = current_key;
        if (key.size() >= 2 && key.front() == '"' && key.back() == '"') {
            key = key.substr(1, key.size() - 2);
        }
        
        std::string val = current_value;
        val.erase(0, val.find_first_not_of(" \t\n\r"));
        val.erase(val.find_last_not_of(" \t\n\r") + 1);
        
        if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
            val = val.substr(1, val.size() - 2);
        }
        
        result += key + ": " + truncate_for_summary(val, 80);
    }
    
    return result;
}

ApprovalResponse prompt_cli_interactive(const ApprovalRequest& request) {
    std::string summary = summarize_args(request.arguments_json);
    
    std::cerr << "\n";
    std::cerr << "Agent wants to execute: " << request.tool_name << "\n";
    std::cerr << "   " << summary << "\n";
    std::cerr << "   [Y]es / [N]o / [A]lways for " << request.tool_name << ": ";
    std::cerr.flush();
    
    std::string line;
    if (!std::getline(std::cin, line)) {
        return ApprovalResponse::No;
    }
    
    return approval_response_from_string(line);
}

}
}
