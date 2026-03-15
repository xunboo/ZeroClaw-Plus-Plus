#include "estop.hpp"
#include "otp.hpp"
#include "policy.hpp"
#include "nlohmann/json.hpp"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <random>
#include <iostream>

namespace zeroclaw {
namespace security {

// ── Helpers ──────────────────────────────────────────────────────

static std::vector<std::string> dedup_sort(const std::vector<std::string>& values) {
    std::vector<std::string> result;
    for (const auto& v : values) {
        std::string trimmed = v;
        size_t start = trimmed.find_first_not_of(" \t\n\r");
        size_t end = trimmed.find_last_not_of(" \t\n\r");
        if (start == std::string::npos) continue;
        trimmed = trimmed.substr(start, end - start + 1);
        if (!trimmed.empty()) result.push_back(trimmed);
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

static std::string now_rfc3339() {
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm utc_tm{};
#ifdef _WIN32
    gmtime_s(&utc_tm, &tt);
#else
    gmtime_r(&tt, &utc_tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

static std::string generate_temp_suffix() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint64_t> dist;
    std::ostringstream oss;
    oss << std::hex << dist(gen);
    return oss.str();
}

// ── EstopState ───────────────────────────────────────────────────

EstopState EstopState::fail_closed() {
    EstopState s;
    s.kill_all = true;
    s.updated_at = now_rfc3339();
    return s;
}

bool EstopState::is_engaged() const {
    return kill_all || network_kill || !blocked_domains.empty() || !frozen_tools.empty();
}

void EstopState::normalize() {
    blocked_domains = dedup_sort(blocked_domains);
    frozen_tools = dedup_sort(frozen_tools);
}

// ── EstopManager ─────────────────────────────────────────────────

std::filesystem::path resolve_state_file_path(const std::filesystem::path& config_dir,
                                               const std::string& state_file) {
    // Expand ~ in path
    std::string expanded = state_file;
    if (!expanded.empty() && expanded[0] == '~') {
        auto h = home_dir();
        if (!h.empty()) {
            if (expanded.size() == 1) {
                expanded = h.string();
            } else if (expanded[1] == '/' || expanded[1] == '\\') {
                expanded = (h / expanded.substr(2)).string();
            }
        }
    }

    std::filesystem::path path(expanded);
    if (path.is_absolute()) return path;
    return config_dir / path;
}

std::string normalize_tool_name(const std::string& raw) {
    std::string value = raw;
    // Trim
    size_t start = value.find_first_not_of(" \t\n\r");
    size_t end = value.find_last_not_of(" \t\n\r");
    if (start == std::string::npos) {
        throw std::runtime_error("Tool name must not be empty");
    }
    value = value.substr(start, end - start + 1);
    // Lowercase
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (value.empty()) {
        throw std::runtime_error("Tool name must not be empty");
    }

    for (char ch : value) {
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_' && ch != '-') {
            throw std::runtime_error("Tool name '" + raw + "' contains invalid characters");
        }
    }
    return value;
}

EstopManager EstopManager::load(const EstopConfig& config, const std::filesystem::path& config_dir) {
    EstopManager manager;
    manager.config_ = config;
    manager.state_path_ = resolve_state_file_path(config_dir, config.state_file);

    bool should_fail_closed = false;

    if (std::filesystem::exists(manager.state_path_)) {
        try {
            std::ifstream file(manager.state_path_);
            std::string content((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());

            auto j = nlohmann::json::parse(content);
            manager.state_.kill_all = j.value("kill_all", false);
            manager.state_.network_kill = j.value("network_kill", false);
            if (j.contains("blocked_domains") && j["blocked_domains"].is_array()) {
                for (const auto& d : j["blocked_domains"]) {
                    manager.state_.blocked_domains.push_back(d.get<std::string>());
                }
            }
            if (j.contains("frozen_tools") && j["frozen_tools"].is_array()) {
                for (const auto& t : j["frozen_tools"]) {
                    manager.state_.frozen_tools.push_back(t.get<std::string>());
                }
            }
            if (j.contains("updated_at") && j["updated_at"].is_string()) {
                manager.state_.updated_at = j["updated_at"].get<std::string>();
            }
            manager.state_.normalize();
        } catch (...) {
            std::cerr << "[WARN] Failed to parse estop state file; entering fail-closed mode\n";
            should_fail_closed = true;
            manager.state_ = EstopState::fail_closed();
        }
    }

    manager.state_.normalize();

    if (should_fail_closed) {
        try { manager.persist_state(); } catch (...) {}
    }

    return manager;
}

void EstopManager::engage(const EstopAction& action) {
    switch (action.type) {
        case EstopAction::Type::KillAll:
            state_.kill_all = true;
            break;
        case EstopAction::Type::NetworkKill:
            state_.network_kill = true;
            break;
        case EstopAction::Type::DomainBlock:
            for (const auto& domain : action.items) {
                std::string normalized = domain;
                size_t s = normalized.find_first_not_of(" \t");
                size_t e = normalized.find_last_not_of(" \t");
                if (s != std::string::npos) normalized = normalized.substr(s, e - s + 1);
                std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                DomainMatcher::validate_pattern(normalized);
                state_.blocked_domains.push_back(normalized);
            }
            break;
        case EstopAction::Type::ToolFreeze:
            for (const auto& tool : action.items) {
                state_.frozen_tools.push_back(normalize_tool_name(tool));
            }
            break;
    }

    state_.updated_at = now_rfc3339();
    state_.normalize();
    persist_state();
}

void EstopManager::resume(const ResumeSelector& selector,
                           const std::optional<std::string>& otp_code,
                           const OtpValidator* otp_validator) {
    ensure_resume_is_authorized(otp_code, otp_validator);

    switch (selector.type) {
        case ResumeSelector::Type::KillAll:
            state_.kill_all = false;
            break;
        case ResumeSelector::Type::Network:
            state_.network_kill = false;
            break;
        case ResumeSelector::Type::Domains: {
            std::vector<std::string> normalized;
            for (const auto& d : selector.items) {
                std::string n = d;
                size_t s = n.find_first_not_of(" \t");
                size_t e = n.find_last_not_of(" \t");
                if (s != std::string::npos) n = n.substr(s, e - s + 1);
                std::transform(n.begin(), n.end(), n.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                normalized.push_back(n);
            }
            state_.blocked_domains.erase(
                std::remove_if(state_.blocked_domains.begin(), state_.blocked_domains.end(),
                               [&normalized](const std::string& existing) {
                                   return std::find(normalized.begin(), normalized.end(), existing)
                                          != normalized.end();
                               }),
                state_.blocked_domains.end());
            break;
        }
        case ResumeSelector::Type::Tools: {
            std::vector<std::string> normalized;
            for (const auto& t : selector.items) {
                normalized.push_back(normalize_tool_name(t));
            }
            state_.frozen_tools.erase(
                std::remove_if(state_.frozen_tools.begin(), state_.frozen_tools.end(),
                               [&normalized](const std::string& existing) {
                                   return std::find(normalized.begin(), normalized.end(), existing)
                                          != normalized.end();
                               }),
                state_.frozen_tools.end());
            break;
        }
    }

    state_.updated_at = now_rfc3339();
    state_.normalize();
    persist_state();
}

void EstopManager::ensure_resume_is_authorized(const std::optional<std::string>& otp_code,
                                                const OtpValidator* otp_validator) {
    if (!config_.require_otp_to_resume) return;

    std::string code;
    if (otp_code.has_value()) {
        code = otp_code.value();
        size_t s = code.find_first_not_of(" \t");
        size_t e = code.find_last_not_of(" \t");
        if (s != std::string::npos) code = code.substr(s, e - s + 1);
    }
    if (code.empty()) {
        throw std::runtime_error("OTP code is required to resume estop state");
    }
    if (!otp_validator) {
        throw std::runtime_error("OTP validator is required to resume estop state with OTP enabled");
    }
    if (!otp_validator->validate(code)) {
        throw std::runtime_error("Invalid OTP code; estop resume denied");
    }
}

void EstopManager::persist_state() {
    auto parent = state_path_.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    nlohmann::json j;
    j["kill_all"] = state_.kill_all;
    j["network_kill"] = state_.network_kill;
    j["blocked_domains"] = state_.blocked_domains;
    j["frozen_tools"] = state_.frozen_tools;
    if (state_.updated_at) j["updated_at"] = *state_.updated_at;

    std::string body = j.dump(2);
    auto temp_path = state_path_;
    temp_path += ".tmp-" + generate_temp_suffix();

    std::ofstream file(temp_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to write temporary estop state file " + temp_path.string());
    }
    file << body;
    file.close();

    std::error_code ec;
    std::filesystem::rename(temp_path, state_path_, ec);
    if (ec) {
        throw std::runtime_error("Failed to atomically replace estop state file " +
                                  state_path_.string() + ": " + ec.message());
    }
}

} // namespace security
} // namespace zeroclaw
